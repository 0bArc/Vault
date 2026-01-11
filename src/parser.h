#pragma once

#include "ast.h"
#include "lexer.h"

#include <vector>

class Parser {
  public:
    explicit Parser(std::vector<Line> lines) : lines_(std::move(lines)) {}
    std::vector<VaultBlock> parse();

  private:
    VaultBlock parse_vault();
    std::vector<Statement> parse_block(int indent);
    Statement parse_statement();

    std::vector<Line> lines_;
    std::size_t pos_{};
};
