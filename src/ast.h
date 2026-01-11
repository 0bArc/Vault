#pragma once

#include <optional>
#include <string>
#include <vector>

struct Target {
    std::optional<std::string> registry;
    std::string key;
};

enum class ValueKind { Literal, Builtin, Document };

struct ValueExpr {
    ValueKind kind{};
    std::string text; // literal value, builtin name, or document body
};

enum class StatementType { Registry, If, Store, Replace, Note, Secure };

struct Statement;

struct IfStmt {
    bool isMissing{}; // true => missing, false => present
    Target target;
    std::vector<Statement> body;
};

struct Statement {
    StatementType type{};
    int line{};
    std::string registryName; // for registry
    Target target;            // for if/store/replace
    ValueExpr value;          // for store/replace
    IfStmt conditional;       // for if
    std::string note;         // for note
};

struct VaultBlock {
    bool optional{};
    std::string name;
    int line{};
    std::vector<Statement> body;
};
