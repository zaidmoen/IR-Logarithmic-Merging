// test_main — PROVIDED unit tests. Tests marked "provided" check the given
// code and pass from the start; the "log" tests check your task.
// Run:  ./test_main data/tiny.txt <work_dir>
// A larger corpus can be passed too; the tests scale the limits to it.
#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>

#include "ir/dynamic.hpp"
#include "ir/reference.hpp"
#include "ir/memory_index.hpp"
#include "ir/merge.hpp"

using namespace ir;
namespace fs = std::filesystem;

static int failures = 0, passed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::cout << "  FAIL: " << msg << "  [" #cond "]\n";          \
            failures++;                                                   \
            return;                                                       \
        }                                                                 \
    } while (0)

static void run(const std::string& name, const std::function<void()>& f) {
    int before = failures;
    try {
        f();
    } catch (const std::exception& e) {
        std::cout << "  FAIL: exception: " << e.what() << "\n";
        failures++;
    }
    if (failures == before) {
        passed++;
        std::cout << "[ ok ] " << name << "\n";
    } else {
        std::cout << "[FAIL] " << name << "\n";
    }
}

int main(int argc, char** argv) {
    std::string corpus = argc > 1 ? argv[1] : "data/tiny.txt";
    std::string work = argc > 2 ? argv[2] : "test_work";
    fs::remove_all(work);
    fs::create_directories(work);

    auto ref = build_reference(corpus);
    std::uint64_t total = 0;
    for (const auto& kv : ref) total += kv.second.size();
    std::cout << "corpus: " << ref.size() << " terms, " << total << " postings\n";

    // Smallest limit any single document allows.
    std::size_t max_doc = 0, num_docs = 0;
    {
        CorpusReader c(corpus);
        DocId id;
        std::string text;
        std::vector<std::string> tok;
        while (c.next(id, text)) {
            tokenize(text, tok);
            max_doc = std::max(max_doc, MemoryIndex::postings_in(tok));
            num_docs++;
        }
    }

    // Sizes are scaled so that a large corpus still gives about 40 pieces.
    const std::size_t base = std::max<std::size_t>(max_doc, total / 40);

    // Splits the corpus into on-disk pieces of at most `limit` postings each,
    // using the provided MemoryIndex (the same way Z0 is flushed).
    auto split_corpus = [&](std::size_t limit, const fs::path& dir) {
        fs::create_directories(dir);
        std::vector<std::string> pieces;
        MemoryIndex m;
        CorpusReader c(corpus);
        DocId id;
        std::string text;
        std::vector<std::string> tok;
        auto flush = [&] {
            if (m.empty()) return;
            pieces.push_back((dir / ("piece_" + std::to_string(pieces.size()))).string());
            m.write_to(pieces.back());
            m.clear();
        };
        while (c.next(id, text)) {
            tokenize(text, tok);
            if (m.size() + MemoryIndex::postings_in(tok) > limit) flush();
            m.add_document(id, tok);
        }
        flush();
        return pieces;
    };

    // ---------------------------------------------------------------- merge
    for (std::size_t limit : {base, 2 * base, std::size_t(total + 1)}) {
        run("provided merge equals reference index (pieces of " + std::to_string(limit) + " postings)", [&] {
            fs::path dir = fs::path(work) / ("merge_" + std::to_string(limit));
            auto pieces = split_corpus(limit, dir);
            if (limit == base && ref.size() > 1)
                CHECK(pieces.size() > 2, "expected several pieces");
            std::string out = (dir / "final").string();
            io_stats().reset();
            merge_indexes(pieces, out);
            CHECK(io_stats().postings_written == total,
                  "a single k-way merge writes every posting exactly once (wrote " +
                      std::to_string(io_stats().postings_written) + ", expected " +
                      std::to_string(total) + ")");
            std::string diff = compare_with_reference(out, ref);
            CHECK(diff.empty(), diff);
        });
    }

    run("provided merge with inputs in reverse docID order", [&] {
        fs::path dir = fs::path(work) / ("merge_" + std::to_string(base));
        std::vector<std::string> pieces;
        for (std::size_t i = 0; index_exists((dir / ("piece_" + std::to_string(i))).string()); ++i)
            pieces.push_back((dir / ("piece_" + std::to_string(i))).string());
        std::reverse(pieces.begin(), pieces.end());
        std::string out = (dir / "final_rev").string();
        merge_indexes(pieces, out);
        std::string diff = compare_with_reference(out, ref);
        CHECK(diff.empty(), diff);
    });

    run("provided merge of overlapping inputs removes duplicates", [&] {
        std::string a = work + "/ov_a", b = work + "/ov_b", o = work + "/ov_out";
        {
            IndexWriter w(a);
            w.add("x", {1, 3, 5});
            w.add("y", {2});
        }
        {
            IndexWriter w(b);
            w.add("w", {9});
            w.add("x", {3, 4, 6});
        }
        merge_indexes({a, b}, o);
        IndexReader r(o);
        PostingsList p;
        CHECK(r.dictionary().size() == 3, "expected terms w, x, y");
        r.get("x", p);
        CHECK((p == PostingsList{1, 3, 4, 5, 6}), "x should be 1 3 4 5 6");
    });

    // ---------------------------------------------------------------- dynamic
    auto dynamic_test = [&](const std::string& mode, std::size_t n) {
        run((mode == "aux" ? "provided aux" : "log") + std::string(": lookup is correct after every document (n=") + std::to_string(n) + ")", [&] {
            fs::path dir = fs::path(work) / (mode + "_" + std::to_string(n));
            fs::create_directories(dir);
            std::unique_ptr<DynamicIndex> idx;
            if (mode == "aux") idx = std::make_unique<AuxMainIndex>(dir.string(), n);
            else idx = std::make_unique<LogarithmicIndex>(dir.string(), n);
            std::map<std::string, PostingsList> partial;
            CorpusReader c(corpus);
            DocId id;
            std::string text;
            std::vector<std::string> tok;
            std::size_t docs = 0;
            while (c.next(id, text)) {
                tokenize(text, tok);
                idx->add_document(id, tok);
                for (const auto& t : tok) {
                    auto& p = partial[t];
                    if (p.empty() || p.back() != id) p.push_back(id);
                }
                ++docs;
                // tiny corpora: check every term after every document;
                // large corpora: ~25 checkpoints, ~40 sampled terms each
                bool tiny = num_docs <= 200;
                if (!tiny && docs % (num_docs / 25) != 0) continue;
                std::size_t step = tiny ? 1 : std::max<std::size_t>(1, partial.size() / 40);
                std::size_t k = 0;
                for (const auto& [term, list] : partial)
                    if (k++ % step == 0)
                        CHECK(idx->lookup(term) == list,
                              "after doc " + std::to_string(id) + ", lookup('" + term + "') is wrong");
                CHECK(idx->lookup("zzzz_not_a_term").empty(), "absent term must give empty list");
            }
            if (mode == "aux") CHECK(idx->disk_indexes() <= 1, "aux keeps at most one disk index");
            if (mode == "log") {
                auto* log = dynamic_cast<LogarithmicIndex*>(idx.get());
                CHECK(idx->stats().flushes > 0 || total <= n, "expected at least one flush");
                CHECK(log->generation_mask() == idx->stats().flushes,
                      "generation mask must equal the flush count in binary (got mask " +
                          std::to_string(log->generation_mask()) + ", flushes " +
                          std::to_string(idx->stats().flushes) + ")");
            }
            std::string out = (dir / "final").string();
            idx->finalize(out);
            std::string diff = compare_with_reference(out, ref);
            CHECK(diff.empty(), "finalize: " + diff);
        });
    };
    for (std::size_t n : {base, base + 3, 3 * base}) {
        dynamic_test("aux", n);
        dynamic_test("log", n);
    }

    run("log: merges write O(T log(T/n)), aux writes O(T^2/n)", [&] {
        std::size_t n = base;
        std::uint64_t written[2];
        int k = 0;
        for (std::string mode : {"aux", "log"}) {
            fs::path dir = fs::path(work) / ("cost_" + mode);
            fs::create_directories(dir);
            std::unique_ptr<DynamicIndex> idx;
            if (mode == "aux") idx = std::make_unique<AuxMainIndex>(dir.string(), n);
            else idx = std::make_unique<LogarithmicIndex>(dir.string(), n);
            io_stats().reset();
            CorpusReader c(corpus);
            DocId id;
            std::string text;
            std::vector<std::string> tok;
            while (c.next(id, text)) {
                tokenize(text, tok);
                idx->add_document(id, tok);
            }
            written[k++] = io_stats().postings_written;
        }
        CHECK(written[1] <= written[0], "log merging should not write more than aux+main");
    });

    std::cout << "\n" << passed << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
