#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: depend <file.svau>\n";
        return 1;
    }
    std::filesystem::path path(argv[1]);
    if (!std::filesystem::exists(path)) {
        std::cerr << "Missing file: " << path << "\n";
        return 1;
    }
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Unable to read: " << path << "\n";
        return 1;
    }
    std::set<std::string> deps;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("depends ", 0) == 0) {
            deps.insert(line.substr(8));
        }
    }

    std::cout << "dependencies for " << path.filename().string() << "\n";
    if (deps.empty()) {
        std::cout << "(none)\n";
        return 0;
    }
    for (const auto &d : deps) {
        std::cout << "- " << d << "\n";
    }
    return 0;
}
