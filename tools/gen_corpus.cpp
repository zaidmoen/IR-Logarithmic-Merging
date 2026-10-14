// gen_corpus — PROVIDED. Generates a synthetic collection, one document per
// line, whose term frequencies follow Zipf's law (IIR §5.1.2).
//   gen_corpus <out.txt> <num_docs> [vocab_size=50000] [avg_doc_len=150] [seed=42]
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

static std::string word_for_rank(std::size_t r) {     // 0 -> "a", 25 -> "z", 26 -> "ba", ...
    std::string w;
    do {
        w.push_back(static_cast<char>('a' + r % 26));
        r /= 26;
    } while (r > 0);
    return w;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: gen_corpus <out.txt> <num_docs> [vocab=50000] [avg_len=150] [seed=42]\n";
        return 2;
    }
    std::size_t docs = std::stoull(argv[2]);
    std::size_t vocab = argc > 3 ? std::stoull(argv[3]) : 50000;
    double avg_len = argc > 4 ? std::stod(argv[4]) : 150;
    unsigned seed = argc > 5 ? static_cast<unsigned>(std::stoul(argv[5])) : 42u;

    std::vector<double> w(vocab);
    for (std::size_t i = 0; i < vocab; ++i) w[i] = 1.0 / static_cast<double>(i + 1);  // cf_i ~ 1/i
    std::discrete_distribution<std::size_t> zipf(w.begin(), w.end());
    std::poisson_distribution<int> len(avg_len);
    std::mt19937 rng(seed);

    std::vector<std::string> words(vocab);
    for (std::size_t i = 0; i < vocab; ++i) words[i] = word_for_rank(i);

    std::ofstream out(argv[1]);
    for (std::size_t d = 0; d < docs; ++d) {
        int L = std::max(1, len(rng));
        for (int k = 0; k < L; ++k) {
            if (k) out << ' ';
            out << words[zipf(rng)];
        }
        out << '\n';
    }
    std::cerr << "wrote " << docs << " documents to " << argv[1] << "\n";
    return 0;
}
