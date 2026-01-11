#pragma once

#include "ast.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

struct SealedEntry {
    std::string digest;
    std::string cipher;
};

struct SealedRegistry {
    std::unordered_map<std::string, SealedEntry> entries;
};

struct SealedVault {
    std::string name;
    bool optional{};
    bool sealed{};
    std::string masterKeyHex;
    std::unordered_map<std::string, SealedRegistry> registries;
};

struct InterpreterOptions {
    bool verbose{false};
    bool materializeOptional{false};
    std::optional<std::string> forcedMasterKey;
};

class Interpreter {
  public:
    explicit Interpreter(InterpreterOptions opts);
        void seed(const std::vector<SealedVault> &existing);
    std::vector<SealedVault> run(const std::vector<VaultBlock> &program);

  private:
    void evaluate_vault(const VaultBlock &vault);
    void execute_statement(const Statement &s);
    bool is_present(const Target &t, int line);
    std::string resolve_registry(const Target &t, int line);
    std::string builtin_value(const ValueExpr &v);

    InterpreterOptions opts_{};
    std::vector<SealedVault> sealed_;
    std::unordered_map<std::string, SealedVault> byName_;
    std::string currentVault_;
    std::optional<std::string> currentRegistry_;
};
