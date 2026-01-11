#include "parser.h"

#include "ast.h"
#include "lexer.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace {
std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(' ');
    return s.substr(start, end - start + 1);
}

bool starts_with(const std::string &s, const std::string &prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string expect_quoted(const std::string &text, int line) {
    auto t = trim(text);
    if (t.size() < 2 || t.front() != '"' || t.back() != '"') {
        throw std::runtime_error("Expected quoted string on line " + std::to_string(line));
    }
    return t.substr(1, t.size() - 2);
}

Target parse_target(const std::string &text, int line) {
    auto expr = trim(text);
    auto arrow = expr.find("->");
    if (arrow == std::string::npos) {
        throw std::runtime_error("Expected '->' in target on line " + std::to_string(line));
    }
    std::string left = trim(expr.substr(0, arrow));
    std::string right = trim(expr.substr(arrow + 2));
    Target t;
    if (!left.empty() && left != "->") t.registry = left;
    t.key = expect_quoted(right, line);
    return t;
}

ValueExpr parse_value_expr(const std::string &text, int line) {
    auto t = trim(text);
    if (t.empty()) throw std::runtime_error("Missing value on line " + std::to_string(line));
    if (t.front() == '"') return {ValueKind::Literal, expect_quoted(t, line)};
    // document literal: starts with { or [ and consumes the rest of the line
    if (!t.empty() && (t.front() == '{' || t.front() == '[')) {
        return {ValueKind::Document, t};
    }
    auto open = t.find('(');
    auto close = t.find(')');
    if (open != std::string::npos && close == t.size() - 1 && open == close - 1) {
        std::string name = t.substr(0, open);
        if (name.empty()) throw std::runtime_error("Bad builtin on line " + std::to_string(line));
        return {ValueKind::Builtin, name};
    }
    throw std::runtime_error("Unrecognized value expression on line " + std::to_string(line));
}
}

std::vector<VaultBlock> Parser::parse() {
    std::vector<VaultBlock> program;
    while (pos_ < lines_.size()) {
        auto &line = lines_[pos_];
        if (trim(line.text).empty()) { pos_++; continue; }
        if (line.indent != 0) {
            throw std::runtime_error("Top-level statements must start at indent 0 (line " + std::to_string(line.number) + ")");
        }
        program.push_back(parse_vault());
    }
    return program;
}

VaultBlock Parser::parse_vault() {
    auto &line = lines_[pos_];
    auto body = trim(line.text);
    bool optional = false;
    std::string name;
    if (starts_with(body, "vault? ")) {
        optional = true;
        name = trim(body.substr(7));
    } else if (starts_with(body, "vault ")) {
        name = trim(body.substr(6));
    } else {
        throw std::runtime_error("Expected 'vault' declaration on line " + std::to_string(line.number));
    }
    if (name.empty()) throw std::runtime_error("Vault name missing on line " + std::to_string(line.number));
    pos_++;
    auto bodyStatements = parse_block(line.indent + 2);
    bool hasSecure = !bodyStatements.empty() && bodyStatements.back().type == StatementType::Secure;
    if (!hasSecure) {
        throw std::runtime_error("Vault '" + name + "' missing terminating 'secure' (line " + std::to_string(line.number) + ")");
    }
    return VaultBlock{optional, name, line.number, std::move(bodyStatements)};
}

std::vector<Statement> Parser::parse_block(int indent) {
    std::vector<Statement> stmts;
    while (pos_ < lines_.size()) {
        auto &line = lines_[pos_];
        if (trim(line.text).empty()) { pos_++; continue; }
        if (line.indent < indent) break;
        if (line.indent != indent) {
            throw std::runtime_error("Unexpected indent on line " + std::to_string(line.number));
        }
        auto parsed = parse_statement();
        stmts.push_back(std::move(parsed));
    }
    return stmts;
}

Statement Parser::parse_statement() {
    auto &line = lines_[pos_];
    auto text = trim(line.text);

    if (starts_with(text, "registry ")) {
        Statement s;
        s.type = StatementType::Registry;
        s.line = line.number;
        s.registryName = trim(text.substr(9));
        if (s.registryName.empty()) throw std::runtime_error("Registry name missing on line " + std::to_string(line.number));
        pos_++;
        return s;
    }

    if (starts_with(text, "if " )) {
        Statement s;
        s.type = StatementType::If;
        s.line = line.number;
        auto rest = trim(text.substr(3));
        if (starts_with(rest, "missing ")) {
            s.conditional.isMissing = true;
            rest = trim(rest.substr(8));
        } else if (starts_with(rest, "present ")) {
            s.conditional.isMissing = false;
            rest = trim(rest.substr(8));
        } else {
            throw std::runtime_error("Expected 'missing' or 'present' on line " + std::to_string(line.number));
        }
        s.conditional.target = parse_target(rest, line.number);
        pos_++;
        s.conditional.body = parse_block(line.indent + 2);
        return s;
    }

    if (starts_with(text, "store ")) {
        Statement s;
        s.type = StatementType::Store;
        s.line = line.number;
        auto rest = trim(text.substr(6));
        auto eq = rest.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Missing '=' on line " + std::to_string(line.number));
        auto targetText = trim(rest.substr(0, eq));
        auto valueText = trim(rest.substr(eq + 1));
        s.target = parse_target(targetText, line.number);
        s.value = parse_value_expr(valueText, line.number);
        pos_++;
        return s;
    }

    if (starts_with(text, "replace ")) {
        Statement s;
        s.type = StatementType::Replace;
        s.line = line.number;
        auto rest = trim(text.substr(8));
        auto eq = rest.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Missing '=' on line " + std::to_string(line.number));
        s.target = parse_target(trim(rest.substr(0, eq)), line.number);
        s.value = parse_value_expr(trim(rest.substr(eq + 1)), line.number);
        pos_++;
        return s;
    }

    if (starts_with(text, "note ")) {
        Statement s;
        s.type = StatementType::Note;
        s.line = line.number;
        s.note = expect_quoted(text.substr(5), line.number);
        pos_++;
        return s;
    }

    if (text == "secure") {
        Statement s;
        s.type = StatementType::Secure;
        s.line = line.number;
        pos_++;
        return s;
    }

    throw std::runtime_error("Unknown statement on line " + std::to_string(line.number) + ": " + text);
}
