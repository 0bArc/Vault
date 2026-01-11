#include "interpreter.h"

#include "ast.h"
#include "crypto.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

Interpreter::Interpreter(InterpreterOptions opts) : opts_(opts) {}

void Interpreter::seed(const std::vector<SealedVault> &existing) {
    byName_.clear();
    sealed_.clear();
    for (const auto &v : existing) {
        byName_[v.name] = v;
    }
}

std::vector<SealedVault> Interpreter::run(const std::vector<VaultBlock> &program) {
    sealed_.clear();
    for (const auto &vault : program) {
        evaluate_vault(vault);
    }
    std::vector<SealedVault> out;
    out.reserve(sealed_.size());
    for (auto &v : sealed_) out.push_back(std::move(v));
    return out;
}

void Interpreter::evaluate_vault(const VaultBlock &vault) {
    currentRegistry_.reset();
    currentVault_.clear();
    auto found = byName_.find(vault.name);
    bool exists = found != byName_.end();
    if (vault.optional && !exists && !opts_.materializeOptional) {
        if (opts_.verbose) std::cout << "[skip] optional vault '" << vault.name << "' not present\n";
        return;
    }

    if (!exists) {
        SealedVault fresh;
        fresh.name = vault.name;
        fresh.optional = vault.optional;
        fresh.sealed = false;
        if (opts_.forcedMasterKey) {
            fresh.masterKeyHex = *opts_.forcedMasterKey;
        } else {
            fresh.masterKeyHex = crypto::random_key_hex();
        }
        byName_[vault.name] = fresh;
    } else {
        if (opts_.forcedMasterKey && found->second.masterKeyHex != *opts_.forcedMasterKey) {
            throw std::runtime_error("Master key mismatch for vault '" + vault.name + "'");
        }
        // Allow re-running scripts against existing sealed vaults by unsealing for this run.
        auto &existingVault = found->second;
        existingVault.optional = vault.optional;
        existingVault.sealed = false;
    }

    currentVault_ = vault.name;
    if (opts_.verbose) std::cout << "[vault] " << (vault.optional ? "optional " : "required ") << vault.name << "\n";

    for (const auto &stmt : vault.body) {
        execute_statement(stmt);
    }

    auto &stored = byName_.at(vault.name);
    sealed_.push_back(stored);
}

bool Interpreter::is_present(const Target &t, int line) {
    auto &vault = byName_.at(currentVault_);
    auto regName = resolve_registry(t, line);
    auto regIt = vault.registries.find(regName);
    if (regIt == vault.registries.end()) return false;
    return regIt->second.entries.find(t.key) != regIt->second.entries.end();
}

std::string Interpreter::resolve_registry(const Target &t, int line) {
    if (t.registry) return *t.registry;
    if (currentRegistry_) return *currentRegistry_;
    throw std::runtime_error("No active registry for target on line " + std::to_string(line));
}

std::string Interpreter::builtin_value(const ValueExpr &v) {
    if (v.kind == ValueKind::Literal) return v.text;
    if (v.kind == ValueKind::Document) return v.text;
    if (v.text == "generate") {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, 15);
        std::string out;
        out.reserve(32);
        for (int i = 0; i < 32; ++i) {
            out.push_back("0123456789abcdef"[dist(rng)]);
        }
        return out;
    }
    if (v.text == "now") {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
        return oss.str();
    }
    throw std::runtime_error("Unknown builtin: " + v.text);
}

void Interpreter::execute_statement(const Statement &s) {
    auto &vault = byName_.at(currentVault_);
    switch (s.type) {
    case StatementType::Registry:
        if (vault.sealed) throw std::runtime_error("Cannot select registry after secure (line " + std::to_string(s.line) + ")");
        currentRegistry_ = s.registryName;
        if (opts_.verbose) std::cout << "  [registry] " << s.registryName << "\n";
        break;
    case StatementType::If: {
        bool present = is_present(s.conditional.target, s.line);
        bool cond = s.conditional.isMissing ? !present : present;
        if (opts_.verbose) {
            std::cout << "  [if] " << (s.conditional.isMissing ? "missing " : "present ")
                      << "-> '" << s.conditional.target.key << "' => " << (cond ? "true" : "false") << "\n";
        }
        if (cond) {
            for (const auto &inner : s.conditional.body) execute_statement(inner);
        }
        break;
    }
    case StatementType::Store: {
        if (vault.sealed) throw std::runtime_error("Cannot store after secure (line " + std::to_string(s.line) + ")");
        auto regName = resolve_registry(s.target, s.line);
        auto &reg = vault.registries[regName];
        if (reg.entries.count(s.target.key)) {
            throw std::runtime_error("store would overwrite existing key on line " + std::to_string(s.line));
        }
        auto plain = builtin_value(s.value);
        auto salt = regName + ":" + s.target.key;
        auto cipher = crypto::encrypt(plain, vault.masterKeyHex, salt);
        auto mac = crypto::digest(cipher, vault.masterKeyHex);
        reg.entries[s.target.key] = {mac, cipher};
        if (opts_.verbose) std::cout << "  [store] " << s.target.key << " (sealed)" << "\n";
        break;
    }
    case StatementType::Replace: {
        if (vault.sealed) throw std::runtime_error("Cannot replace after secure (line " + std::to_string(s.line) + ")");
        auto regName = resolve_registry(s.target, s.line);
        auto &reg = vault.registries[regName];
        auto plain = builtin_value(s.value);
        auto salt = regName + ":" + s.target.key;
        auto cipher = crypto::encrypt(plain, vault.masterKeyHex, salt);
        auto mac = crypto::digest(cipher, vault.masterKeyHex);
        reg.entries[s.target.key] = {mac, cipher};
        if (opts_.verbose) std::cout << "  [replace] " << s.target.key << " (sealed)" << "\n";
        break;
    }
    case StatementType::Note:
        if (opts_.verbose) std::cout << "  [note] " << s.note << "\n";
        break;
    case StatementType::Secure:
        vault.sealed = true;
        if (opts_.verbose) std::cout << "  [secure] vault sealed\n";
        break;
    }
}
