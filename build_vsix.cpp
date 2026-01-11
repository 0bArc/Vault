#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

int main() {
    try {
        auto root = std::filesystem::current_path();
        auto extDir = root / "vscode-extension";
        if (!std::filesystem::exists(extDir)) {
            std::cerr << "vscode-extension folder not found: " << extDir << "\n";
            return 1;
        }
        std::filesystem::current_path(extDir);
#ifdef _WIN32
        _putenv_s("NODE_OPTIONS", "-r ./tools/file-polyfill.js");
#else
        setenv("NODE_OPTIONS", "-r ./tools/file-polyfill.js", 1);
#endif
        std::string cmd = "npx --yes @vscode/vsce@2.24.0 package";
        std::cout << "Running: " << cmd << " in " << std::filesystem::current_path() << "\n";
        int rc = std::system(cmd.c_str());
        std::filesystem::current_path(root);
        if (rc != 0) {
            std::cerr << "vsix packaging failed with code " << rc << "\n";
            return rc;
        }
        std::cout << "VSIX generated in: " << (extDir / "vault-language-support-0.0.1.vsix") << "\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
