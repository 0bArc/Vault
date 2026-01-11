#include "ast.h"
#include "crypto.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <optional>
#include <regex>
#include <string>
#include <vector>
#include <cstdlib>

namespace {
std::string default_output(const std::string &input) {
    auto path = std::filesystem::path(input);
    return path.replace_extension(".svau").string();
}

struct VaultConfig {
    std::string masterKey;
    std::string token;
    std::vector<std::string> securityQuestions;
    std::vector<std::string> securityDigests;
    std::vector<std::string> securityAnswers;
};

struct LoadedArchive {
    std::string token;
    std::string hmac;
    std::vector<std::string> dependencies;
    std::vector<SealedVault> vaults;
};

struct PlainEntry {
    std::string registry;
    std::string key;
    std::string value;
    std::string mac;
};

void print_plain(const LoadedArchive &archive, bool hideMac) {
    std::cout << "# Vault Archive (decrypted view)\n";
    if (!archive.dependencies.empty()) {
        std::cout << "depends";
        for (const auto &d : archive.dependencies) std::cout << " " << d;
        std::cout << "\n";
    }
    for (const auto &v : archive.vaults) {
        std::cout << "vault " << v.name << "\n";
        for (const auto &regPair : v.registries) {
            const auto &regName = regPair.first;
            std::cout << "  registry " << regName << "\n";
            for (const auto &entryPair : regPair.second.entries) {
                const auto &key = entryPair.first;
                const auto &entry = entryPair.second;
                std::string plain = v.sealed
                    ? crypto::decrypt(entry.cipher, v.masterKeyHex, regName + ":" + key)
                    : entry.cipher;
                if (hideMac || !v.sealed) {
                    std::cout << "    " << key << " = \"" << plain << "\"\n";
                } else {
                    std::cout << "    " << key << " = \"" << plain << "\" (mac=" << entry.digest << ")\n";
                }
            }
        }
        std::cout << "---\n";
    }
}

std::vector<std::string> sorted_unique(std::vector<std::string> vals) {
    std::sort(vals.begin(), vals.end());
    vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
    return vals;
}

std::vector<PlainEntry> decrypt_entries(const LoadedArchive &archive) {
    std::vector<PlainEntry> out;
    for (const auto &v : archive.vaults) {
        for (const auto &regPair : v.registries) {
            const auto &regName = regPair.first;
            for (const auto &entryPair : regPair.second.entries) {
                const auto &key = entryPair.first;
                const auto &entry = entryPair.second;
                PlainEntry p;
                p.registry = regName;
                p.key = key;
                p.value = v.sealed
                    ? crypto::decrypt(entry.cipher, v.masterKeyHex, regName + ":" + key)
                    : entry.cipher;
                p.mac = entry.digest;
                out.push_back(std::move(p));
            }
        }
    }
    return out;
}

std::optional<std::string> extract_field(const std::string &doc, const std::string &field) {
    // naive extraction: looks for field: number or field: "string"
    std::regex numRe(field + "\\s*:\\s*([-+]?[0-9]+(?:\\.[0-9]+)?)");
    std::regex strRe(field + "\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    if (std::regex_search(doc, m, numRe) && m.size() > 1) return m[1].str();
    if (std::regex_search(doc, m, strRe) && m.size() > 1) return m[1].str();
    return std::nullopt;
}

void run_script(const std::string &path, const LoadedArchive &archive) {
    auto entries = decrypt_entries(archive);
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Unable to read script: " + path);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    if (lines.empty()) return;

    // Very small DSL: for idx, var in document:find::matching("substr"):
    //   log(var.field)
    auto header = lines.front();
    auto colon = header.find(":find::matching(");
    if (header.rfind("for ", 0) != 0 || colon == std::string::npos) {
        throw std::runtime_error("Unsupported script header");
    }
    auto inPos = header.find(" in ");
    if (inPos == std::string::npos) throw std::runtime_error("Unsupported script header");
    auto vars = header.substr(4, inPos - 4);
    auto comma = vars.find(',');
    if (comma == std::string::npos) throw std::runtime_error("Need two loop vars");
    auto idxVar = std::string(vars.substr(0, comma));
    auto docVar = std::string(vars.substr(comma + 1));
    idxVar.erase(0, idxVar.find_first_not_of(' '));
    idxVar.erase(idxVar.find_last_not_of(' ') + 1);
    docVar.erase(0, docVar.find_first_not_of(' '));
    docVar.erase(docVar.find_last_not_of(' ') + 1);
    auto matchStart = colon + std::string(":find::matching(").size();
    auto end = header.find(')', matchStart);
    if (end == std::string::npos) throw std::runtime_error("Bad matching() syntax");
    auto needle = header.substr(matchStart, end - matchStart);
    if (!needle.empty() && needle.front() == '"' && needle.back() == '"') {
        needle = needle.substr(1, needle.size() - 2);
    }

    std::vector<std::string> body(lines.begin() + 1, lines.end());
    int idx = 0;
    for (const auto &e : entries) {
        if (e.key.find(needle) == std::string::npos) continue;
        for (const auto &b : body) {
            auto trimmed = b;
            trimmed.erase(0, trimmed.find_first_not_of(' '));
            if (trimmed.rfind("log(", 0) == 0 && trimmed.back() == ')') {
                auto inside = trimmed.substr(4, trimmed.size() - 5);
                if (inside == docVar + ".value") {
                    std::cout << e.value << "\n";
                } else if (inside.rfind(docVar + ".", 0) == 0) {
                    auto field = inside.substr(docVar.size() + 1);
                    auto val = extract_field(e.value, field);
                    if (val) std::cout << *val << "\n";
                } else if (inside == idxVar) {
                    std::cout << idx << "\n";
                }
            }
        }
        idx++;
    }
}

VaultConfig load_config(bool requireSecurity) {
    auto path = std::filesystem::path(".vault") / "var.vc";
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Missing config: " + path.string());
    }
    VaultConfig cfg;
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Unable to read config: " + path.string());
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        if (key == "MASTER_KEY") cfg.masterKey = val;
        if (key == "TOKEN") cfg.token = val;
        if (key == "SECURITY_Q1" || key == "SECURITY_Q2" || key == "SECURITY_Q3") {
            cfg.securityQuestions.push_back(val);
        }
        if (key == "SECURITY_Q4") {
            cfg.securityQuestions.push_back(val);
            std::cerr << "Warning: SECURITY_Q4 present; only 3 are recommended\n";
        }
        if (key == "SECURITY_A1_DIGEST" || key == "SECURITY_A2_DIGEST" || key == "SECURITY_A3_DIGEST" || key == "SECURITY_A4_DIGEST") {
            cfg.securityDigests.push_back(val);
        }
        if (key == "SECURITY_A1" || key == "SECURITY_A2" || key == "SECURITY_A3" || key == "SECURITY_A4") {
            cfg.securityAnswers.push_back(val);
        }
    }
    if (cfg.masterKey.empty() || cfg.token.empty()) {
        throw std::runtime_error("Config incomplete: require MASTER_KEY and TOKEN in .vault/var.vc");
    }
    // Enforce up to 3 security answers only when explicitly requested (lost-mode recovery).
    if (requireSecurity) {
        if (cfg.securityQuestions.size() > 3) {
            std::cerr << "Warning: more than 3 security questions; only first 3 are recommended\n";
        }
        std::size_t maxCount = std::max(cfg.securityDigests.size(), cfg.securityAnswers.size());
        if (maxCount == 0) {
            throw std::runtime_error("Security questions/answers required in lost mode");
        }
        if (maxCount > 4) {
            std::cerr << "Warning: more than 4 security entries found; extra will be ignored\n";
            maxCount = 4;
        }
        for (std::size_t i = 0; i < maxCount; ++i) {
            std::string digest;
            if (i < cfg.securityDigests.size()) digest = cfg.securityDigests[i];
            if (i < cfg.securityAnswers.size()) {
                auto computed = crypto::digest(cfg.securityAnswers[i], cfg.masterKey);
                if (!digest.empty() && digest != computed) {
                    throw std::runtime_error("Security answer digest mismatch for slot " + std::to_string(i + 1));
                }
                digest = computed;
            }
            if (digest.empty()) {
                throw std::runtime_error("Missing security answer/digest for slot " + std::to_string(i + 1));
            }
            // At this point digest is validated/derived; nothing more to store.
        }
    }
    return cfg;
}

void write_svau(std::ostream &out, const std::vector<SealedVault> &vaults, const std::string &token, const std::vector<std::string> &dependencies) {
    out << "# Vault Secure Archive\n";
    auto deps = sorted_unique(dependencies);
    for (const auto &d : deps) out << "depends " << d << "\n";
    // hmac is written separately after computation
    for (const auto &v : vaults) {
        out << "vault " << v.name << " (" << (v.optional ? "optional" : "required") << ")\n";
        out << "sealed " << (v.sealed ? "true" : "false") << "\n";
        std::vector<std::string> registryNames;
        registryNames.reserve(v.registries.size());
        for (const auto &regPair : v.registries) registryNames.push_back(regPair.first);
        std::sort(registryNames.begin(), registryNames.end());
        for (const auto &regName : registryNames) {
            const auto &reg = v.registries.at(regName);
            out << "  registry " << regName << "\n";
            std::vector<std::string> entryNames;
            entryNames.reserve(reg.entries.size());
            for (const auto &entry : reg.entries) entryNames.push_back(entry.first);
            std::sort(entryNames.begin(), entryNames.end());
            for (const auto &entryName : entryNames) {
                const auto &entry = reg.entries.at(entryName);
                out << "    entry " << entryName << "\n";
                out << "      digest " << entry.digest << "\n";
                out << "      cipher " << entry.cipher << "\n";
            }
        }
        out << "---\n";
    }
}

void write_svau_file(const std::string &outPath, const std::vector<SealedVault> &vaults, const std::string &token, const std::vector<std::string> &dependencies) {
    std::ofstream out(outPath, std::ios::trunc);
    if (!out) throw std::runtime_error("Unable to write: " + outPath);
    write_svau(out, vaults, token, dependencies);
}

std::string compute_archive_hmac(const std::vector<SealedVault> &vaults, const std::string &token, const std::string &masterKeyHex, const std::vector<std::string> &dependencies) {
    // deterministic serialization (token is implicit secret; not written to archive)
    std::ostringstream oss;
    oss << "token " << token << "\n";
    auto deps = sorted_unique(dependencies);
    for (const auto &d : deps) oss << "depends " << d << "\n";
    for (const auto &v : vaults) {
        oss << "vault " << v.name << " (" << (v.optional ? "optional" : "required") << ")\n";
        oss << "sealed " << (v.sealed ? "true" : "false") << "\n";
        std::vector<std::string> registryNames;
        registryNames.reserve(v.registries.size());
        for (const auto &regPair : v.registries) registryNames.push_back(regPair.first);
        std::sort(registryNames.begin(), registryNames.end());
        for (const auto &regName : registryNames) {
            const auto &reg = v.registries.at(regName);
            oss << "  registry " << regName << "\n";
            std::vector<std::string> entryNames;
            entryNames.reserve(reg.entries.size());
            for (const auto &entry : reg.entries) entryNames.push_back(entry.first);
            std::sort(entryNames.begin(), entryNames.end());
            for (const auto &entryName : entryNames) {
                const auto &entry = reg.entries.at(entryName);
                oss << "    entry " << entryName << "\n";
                oss << "      digest " << entry.digest << "\n";
                oss << "      cipher " << entry.cipher << "\n";
            }
        }
        oss << "---\n";
    }
    return crypto::digest(oss.str(), masterKeyHex);
}

LoadedArchive read_svau(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Unable to read: " + path);
    LoadedArchive result;
    std::vector<SealedVault> vaults;
    std::string line;
    SealedVault current;
    std::string currentReg;
    std::string currentEntryKey;
    auto flush = [&]() {
        if (!current.name.empty()) vaults.push_back(current);
        current = SealedVault{};
        currentReg.clear();
    };

    while (std::getline(in, line)) {
        if (line == "---") { flush(); continue; }
        if (line.empty() || line == "# Vault Secure Archive") continue;
        if (line.rfind("hmac ", 0) == 0) { result.hmac = line.substr(5); continue; }
        if (line.rfind("depends ", 0) == 0) { result.dependencies.push_back(line.substr(8)); continue; }
        if (line.rfind("token ", 0) == 0) { result.token = line.substr(6); continue; }
        if (line.rfind("vault ", 0) == 0) {
            flush();
            std::istringstream iss(line.substr(6));
            std::string name, paren;
            iss >> name;
            current.name = name;
            auto pos = line.find('(');
            current.optional = (pos != std::string::npos && line.find("optional") != std::string::npos);
        } else if (line.rfind("sealed ", 0) == 0) {
            current.sealed = (line.find("true") != std::string::npos);
        } else if (line.rfind("  registry ", 0) == 0) {
            currentReg = line.substr(11);
            current.registries[currentReg] = SealedRegistry{};
        } else if (line.rfind("    entry ", 0) == 0) {
            currentEntryKey = line.substr(10);
            current.registries[currentReg].entries[currentEntryKey] = SealedEntry{};
        } else if (line.rfind("      digest ", 0) == 0) {
            auto &entry = current.registries[currentReg].entries[currentEntryKey];
            entry.digest = line.substr(13);
        } else if (line.rfind("      cipher ", 0) == 0) {
            auto &entry = current.registries[currentReg].entries[currentEntryKey];
            entry.cipher = line.substr(13);
        }
    }
    flush();
    result.vaults = std::move(vaults);
    return result;
}

void usage() {
    std::cerr << "Usage: vaultc <input.vau|input.svau|input.vsc> [--out file.svau] [--stdout] [--hide-mac] [--load file.svau] [--verbose] [--materialize-optionals] [--lost]\n";
}
}

int vaultc_main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    std::string input = argv[1];
    std::string output = default_output(input);
    InterpreterOptions opts{};
    bool emitStdout = true;
    std::optional<std::string> loadPath;
    bool inputIsSvau = std::filesystem::path(input).extension() == ".svau";
    bool inputIsVsc = std::filesystem::path(input).extension() == ".vsc";
    bool hideMac = false;
    bool requireSecurity = false;
    std::vector<std::string> dependencies;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            output = argv[++i];
            emitStdout = false;
        } else if (arg == "--stdout") {
            emitStdout = true;
        } else if (arg == "--hide-mac") {
            hideMac = true;
        } else if (arg == "--load" && i + 1 < argc) {
            loadPath = argv[++i];
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--materialize-optionals") {
            opts.materializeOptional = true;
        } else if (arg == "--lost") {
            requireSecurity = true;
        } else {
            usage();
            return 1;
        }
    }

    try {
        auto cfg = load_config(requireSecurity);
        if (inputIsSvau) {
            auto archive = read_svau(input);
            // token is not stored for new archives; accept only if present and matching
            if (!archive.token.empty() && archive.token != cfg.token) {
                throw std::runtime_error("Token mismatch for archive");
            }
            // inject master key (not stored in archive) and verify hmac using current token
            for (auto &v : archive.vaults) v.masterKeyHex = cfg.masterKey;
            auto want = compute_archive_hmac(archive.vaults, cfg.token, cfg.masterKey, archive.dependencies);
            if (!archive.hmac.empty() && archive.hmac != want) {
                throw std::runtime_error("Archive HMAC verification failed");
            }
            dependencies = archive.dependencies;
            print_plain(archive, hideMac);
        } else if (inputIsVsc) {
            if (!loadPath) throw std::runtime_error("Script requires --load <archive.svau>");
            auto archive = read_svau(*loadPath);
            if (archive.token != cfg.token) throw std::runtime_error("Token mismatch for archive");
            for (auto &v : archive.vaults) v.masterKeyHex = cfg.masterKey;
            auto want = compute_archive_hmac(archive.vaults, archive.token, cfg.masterKey, archive.dependencies);
            if (!archive.hmac.empty() && archive.hmac != want) {
                throw std::runtime_error("Archive HMAC verification failed");
            }
            dependencies = archive.dependencies;
            run_script(input, archive);
        } else {
            auto lines = lex_file(input);
            Parser parser(lines);
            auto program = parser.parse();
            opts.forcedMasterKey = cfg.masterKey;
            Interpreter interp(opts);
            if (loadPath) {
                auto seedArchive = read_svau(*loadPath);
                if (!seedArchive.token.empty() && seedArchive.token != cfg.token) {
                    throw std::runtime_error("Token mismatch for loaded archive");
                }
                for (auto &v : seedArchive.vaults) v.masterKeyHex = cfg.masterKey;
                auto want = compute_archive_hmac(seedArchive.vaults, cfg.token, cfg.masterKey, seedArchive.dependencies);
                if (!seedArchive.hmac.empty() && seedArchive.hmac != want) {
                    throw std::runtime_error("Loaded archive HMAC verification failed");
                }
                dependencies = seedArchive.dependencies;
                dependencies.push_back(std::filesystem::path(*loadPath).filename().string());
                dependencies = sorted_unique(std::move(dependencies));
                interp.seed(seedArchive.vaults);
            }
            auto sealed = interp.run(program);
            auto hmac = compute_archive_hmac(sealed, cfg.token, cfg.masterKey, dependencies);
            if (emitStdout) {
                write_svau(std::cout, sealed, cfg.token, dependencies);
                std::cout << "hmac " << hmac << "\n";
            } else {
                // write file then append hmac line
                write_svau_file(output, sealed, cfg.token, dependencies);
                std::ofstream out(output, std::ios::app);
                out << "hmac " << hmac << "\n";
                if (opts.verbose) std::cout << "wrote " << output << "\n";
            }
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

#ifndef VAULT_NO_MAIN
int main(int argc, char **argv) {
    return vaultc_main(argc, argv);
}
#endif
