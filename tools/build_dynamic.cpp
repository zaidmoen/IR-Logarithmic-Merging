// build_dynamic — PROVIDED driver for both dynamic indexes.
//   build_dynamic <aux|log> <corpus.txt> <work_dir> <n> <out_base> [--trace file.csv]
// With --trace, one CSV row is written after every flush:
//   docs,postings_so_far,postings_written,disk_indexes,generation_mask
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "ir/dynamic.hpp"

using namespace ir;

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "usage: build_dynamic <aux|log> <corpus.txt> <work_dir> <n> <out_base>"
                     " [--trace file.csv]\n";
        return 2;
    }
    std::string mode = argv[1], corpus_path = argv[2], work = argv[3], out = argv[5];
    std::size_t n = std::stoull(argv[4]);
    std::ofstream trace;
    if (argc > 7 && std::string(argv[6]) == "--trace") {
        trace.open(argv[7]);
        trace << "docs,postings_so_far,postings_written,disk_indexes,generation_mask\n";
    }
    std::filesystem::create_directories(work);

    try {
        std::unique_ptr<DynamicIndex> idx;
        if (mode == "aux") idx = std::make_unique<AuxMainIndex>(work, n);
        else if (mode == "log") idx = std::make_unique<LogarithmicIndex>(work, n);
        else throw std::runtime_error("mode must be 'aux' or 'log'");
        auto* log = dynamic_cast<LogarithmicIndex*>(idx.get());

        io_stats().reset();
        auto t0 = std::chrono::steady_clock::now();
        CorpusReader corpus(corpus_path);
        DocId id;
        std::string text;
        std::vector<std::string> tokens;
        std::uint64_t docs = 0, postings = 0, last_flushes = 0;
        while (corpus.next(id, text)) {
            tokenize(text, tokens);
            idx->add_document(id, tokens);
            docs++;
            postings += MemoryIndex::postings_in(tokens);
            if (trace && idx->stats().flushes != last_flushes) {
                last_flushes = idx->stats().flushes;
                trace << docs << ',' << postings << ',' << io_stats().postings_written << ','
                      << idx->disk_indexes() << ',' << (log ? log->generation_mask() : 0) << '\n';
            }
        }
        IOStats before_final = io_stats();
        DynamicStats ds = idx->stats();
        std::size_t disk_before_final = idx->disk_indexes();
        idx->finalize(out);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        std::cout << "mode                    " << mode << "\n"
                  << "n                       " << n << "\n"
                  << "docs                    " << docs << "\n"
                  << "postings (T)            " << postings << "\n"
                  << "flushes                 " << ds.flushes << "\n"
                  << "merges (before final)   " << ds.merges << "\n"
                  << "max_disk_indexes        " << ds.max_disk_indexes << "\n"
                  << "disk_indexes_at_end     " << disk_before_final << "\n"
                  << "postings_written        " << before_final.postings_written
                  << "  (incl. finalize " << io_stats().postings_written << ")\n"
                  << "postings_read           " << before_final.postings_read
                  << "  (incl. finalize " << io_stats().postings_read << ")\n"
                  << "write_amplification     "
                  << (postings ? double(before_final.postings_written) / postings : 0) << "\n"
                  << "seconds                 " << secs << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
