// memory_index.cpp — PROVIDED.
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

#include "ir/memory_index.hpp"

namespace ir {

void MemoryIndex::add_document(DocId id, const std::vector<std::string>& tokens) {
    for (const std::string& t : tokens) {
        PostingsList& list = dict_[t];
        if (!list.empty() && list.back() > id)
            throw std::logic_error("documents must arrive in increasing docID order");
        if (list.empty() || list.back() != id) {   // first occurrence of t in this doc
            list.push_back(id);
            npostings_++;
        }
    }
}

std::size_t MemoryIndex::postings_in(const std::vector<std::string>& tokens) {
    std::unordered_set<std::string> distinct(tokens.begin(), tokens.end());
    return distinct.size();
}

void MemoryIndex::write_to(const std::string& base) const {
    IndexWriter w(base);
    for (const auto& [term, list] : dict_) w.add(term, list);
    w.close();
}

PostingsList MemoryIndex::lookup(const std::string& term) const {
    auto it = dict_.find(term);
    return it == dict_.end() ? PostingsList{} : it->second;
}

void MemoryIndex::clear() {
    dict_.clear();
    npostings_ = 0;
}

}  // namespace ir
