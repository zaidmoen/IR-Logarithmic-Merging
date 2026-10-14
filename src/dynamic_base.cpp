// dynamic_base.cpp — PROVIDED parts of DynamicIndex.
#include <algorithm>
#include <iterator>
#include <stdexcept>

#include "ir/dynamic.hpp"

namespace ir {

void DynamicIndex::add_document(DocId id, const std::vector<std::string>& tokens) {
    std::size_t need = MemoryIndex::postings_in(tokens);
    if (need > n_)
        throw std::runtime_error("document " + std::to_string(id) +
                                 " alone exceeds the auxiliary index size n");
    if (z0_.size() + need > n_) {
        flush();
        stats_.flushes++;
        stats_.max_disk_indexes = std::max<std::uint64_t>(stats_.max_disk_indexes, disk_indexes());
    }
    z0_.add_document(id, tokens);
}

std::string DynamicIndex::new_temp_name() {
    return work_dir_ + "/tmp_" + std::to_string(next_file_++);
}

const IndexReader& ReaderCache::get(const std::string& base) const {
    auto& slot = open_[base];
    if (!slot) slot = std::make_unique<IndexReader>(base);
    return *slot;
}

void ReaderCache::forget(const std::string& base) { open_.erase(base); }

PostingsList union_postings(const PostingsList& a, const PostingsList& b) {
    PostingsList out;
    out.reserve(a.size() + b.size());
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
    return out;
}

PostingsList disk_lookup(const ReaderCache& readers, const std::string& base,
                         const std::string& term) {
    PostingsList out;
    if (!base.empty()) readers.get(base).get(term, out);
    return out;
}

void write_empty_index(const std::string& base) {
    IndexWriter w(base);
    w.close();
}

}  // namespace ir
