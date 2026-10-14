// verify — PROVIDED. Checks an on-disk index against a naive in-memory index.
//   verify <corpus.txt> <index_base>
#include <iostream>

#include "ir/reference.hpp"

using namespace ir;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: verify <corpus.txt> <index_base>\n";
        return 2;
    }
    try {
        auto ref = build_reference(argv[1]);
        std::string diff = compare_with_reference(argv[2], ref);
        if (diff.empty()) {
            std::cout << "OK: " << ref.size() << " terms match the reference index\n";
            return 0;
        }
        std::cout << "MISMATCH: " << diff << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
    return 1;
}
