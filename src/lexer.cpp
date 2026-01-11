#include "lexer.h"

#include <fstream>
#include <stdexcept>
#include <string>

namespace {
std::string trim_left(const std::string &s) {
    auto pos = s.find_first_not_of(' ');
    if (pos == std::string::npos) return "";
    return s.substr(pos);
}
}

std::vector<Line> lex_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Unable to open file: " + path);
    std::vector<Line> lines;
    std::string line;
    int number = 1;
    while (std::getline(in, line)) {
        if (line.find('\t') != std::string::npos) {
            throw std::runtime_error("Tabs are not allowed (line " + std::to_string(number) + ")");
        }
        int indent = 0;
        for (char c : line) {
            if (c == ' ') indent++; else break;
        }
        lines.push_back({number, indent, trim_left(line)});
        number++;
    }
    return lines;
}
