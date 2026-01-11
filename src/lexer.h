#pragma once

#include <string>
#include <vector>

struct Line {
    int number{};
    int indent{};
    std::string text;
};

std::vector<Line> lex_file(const std::string &path);
