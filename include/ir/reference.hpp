// reference.hpp — PROVIDED. A naive in-memory indexer used ONLY by the
// verifier and the tests to check your on-disk indexes. Do not use it in
// your own implementation: it ignores the memory budget on purpose.
#pragma once

#include <map>
#include <string>

#include "ir/common.hpp"
#include "ir/index_io.hpp"

namespace ir {

inline std::map<std::string, PostingsList> build_reference(const std::string& corpus_path) {
    std::map<std::string, PostingsList> idx;
    CorpusReader corpus(corpus_path);
    DocId id;
    std::string text;
    std::vector<std::string> tokens;
    while (corpus.next(id, text)) {
        tokenize(text, tokens);
        for (const std::string& t : tokens) {
            PostingsList& p = idx[t];
            if (p.empty() || p.back() != id) p.push_back(id);
        }
    }
    return idx;
}

// Compares an on-disk index with the reference. Returns "" if identical,
// otherwise a description of the first difference.
inline std::string compare_with_reference(const std::string& index_base,
                                          const std::map<std::string, PostingsList>& ref) {
    IndexCursor c(index_base);
    auto it = ref.begin();
    for (; c.valid() && it != ref.end(); c.next(), ++it) {
        if (c.term() != it->first)
            return "term mismatch: index has '" + c.term() + "', expected '" + it->first + "'";
        if (c.postings() != it->second)
            return "postings differ for term '" + it->first + "' (df " +
                   std::to_string(c.postings().size()) + " vs expected " +
                   std::to_string(it->second.size()) + ")";
    }
    if (c.valid()) return "index has extra term '" + c.term() + "'";
    if (it != ref.end()) return "index is missing term '" + it->first + "'";
    return "";
}

}  // namespace ir
