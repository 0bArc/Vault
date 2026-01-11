#include <iostream>
#include <vector>
#include <string>

// Reuse the compiler's entrypoint without pulling in its default main
int vaultc_main(int argc, char **argv);

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: vault <file.vau|file.svau|file.vsc> [vaultc args...]\n";
        std::cerr << "Example: vault src/examples/depends_test.vau --out build/depends_test.svau --load build/depend.svau\n";
        return 1;
    }

    // Forward argv unchanged so behavior matches vaultc.
    // vaultc_main expects a mutable argv; reuse the incoming buffer.
    return vaultc_main(argc, argv);
}
