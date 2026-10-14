// lookup — PROVIDED. Prints the postings of one or more terms.
//   lookup <index_base> <term> [term ...]
#include <iostream>

#include "ir/index_io.hpp"

using namespace ir;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: lookup <index_base> <term> [term ...]\n";
        return 2;
    }
    try {
        IndexReader r(argv[1]);
        std::cout << r.dictionary().size() << " terms in dictionary\n";
        for (int i = 2; i < argc; ++i) {
            std::vector<std::string> tok;
            tokenize(argv[i], tok);
            std::string term = tok.empty() ? argv[i] : tok[0];
            PostingsList p;
            r.get(term, p);
            std::cout << term << " (df=" << p.size() << "):";
            for (std::size_t k = 0; k < p.size() && k < 20; ++k) std::cout << ' ' << p[k];
            if (p.size() > 20) std::cout << " ...";
            std::cout << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
