// memory_index.hpp — the in-memory auxiliary index Z0.
// PROVIDED: you do not need to change this file.
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "ir/common.hpp"
#include "ir/index_io.hpp"

namespace ir {

// In-memory inverted index for the documents added since the last flush.
// size() counts POSTINGS, i.e. distinct (term, docID) pairs. This is the
// quantity the auxiliary index size n is expressed in.
class MemoryIndex {
public:
    // Adds one document. Documents must arrive in increasing docID order.
    void add_document(DocId id, const std::vector<std::string>& tokens);

    // Number of postings document `tokens` would add (its distinct terms).
    static std::size_t postings_in(const std::vector<std::string>& tokens);

    std::size_t size() const { return npostings_; }
    bool empty() const { return npostings_ == 0; }

    // Writes the terms in sorted order as an on-disk index; does NOT clear.
    void write_to(const std::string& base) const;
    // Returns the postings of `term` (empty list if absent).
    PostingsList lookup(const std::string& term) const;
    void clear();

private:
    std::map<std::string, PostingsList> dict_;   // std::map keeps terms sorted
    std::size_t npostings_ = 0;
};

}  // namespace ir
