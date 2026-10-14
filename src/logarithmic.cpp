// logarithmic.cpp — logarithmic merging for a dynamic inverted index
#include "ir/dynamic.hpp"

#include <algorithm>

using namespace std;

namespace ir {

LogarithmicIndex::~LogarithmicIndex() {
    // Remove every temporary generation that was not moved by finalize
    for (const string& base : gen_) {
        if (!base.empty()) {
            readers_.forget(base);
            remove_index(base);
        }
    }
}

void LogarithmicIndex::flush() {
    if (z0_.empty()) return;

    // Start the carry with the newest auxiliary index
    string carry = new_temp_name();
    z0_.write_to(carry);
    z0_.clear();

    for (size_t level = 0; ; ++level) {
        if (level == gen_.size()) gen_.push_back("");

        if (gen_[level].empty()) {
            gen_[level] = carry;
            return;
        }

        // This generation is older, so it must be the first merge input
        string merged = new_temp_name();
        merge_indexes({gen_[level], carry}, merged);
        stats_.merges++;

        readers_.forget(gen_[level]);
        readers_.forget(carry);
        remove_index(gen_[level]);
        remove_index(carry);

        gen_[level].clear();
        carry = merged;
    }
}

PostingsList LogarithmicIndex::lookup(const string& term) const {
    PostingsList result = z0_.lookup(term);

    // Each generation covers a different part of the document stream
    for (const string& base : gen_) {
        if (!base.empty()) {
            result = union_postings(result, disk_lookup(readers_, base, term));
        }
    }

    return result;
}

void LogarithmicIndex::finalize(const string& out_base) {
    flush();

    vector<string> inputs;
    // Higher levels contain older documents, so keep merge input order old to new
    for (size_t level = gen_.size(); level > 0; --level) {
        const string& base = gen_[level - 1];
        if (!base.empty()) inputs.push_back(base);
    }

    if (inputs.empty()) {
        write_empty_index(out_base);
        return;
    }

    if (inputs.size() == 1) {
        readers_.forget(inputs[0]);
        rename_index(inputs[0], out_base);
    } else {
        // Finish with one k-way merge instead of repeatedly rewriting old data
        merge_indexes(inputs, out_base);
        stats_.merges++;

        for (const string& base : inputs) {
            readers_.forget(base);
            remove_index(base);
        }
    }

    for (string& base : gen_) base.clear();
}

size_t LogarithmicIndex::disk_indexes() const {
    size_t count = 0;
    for (const string& base : gen_) {
        if (!base.empty()) ++count;
    }
    return count;
}

uint64_t LogarithmicIndex::generation_mask() const {
    uint64_t mask = 0;
    for (size_t level = 0; level < gen_.size() && level < 64; ++level) {
        if (!gen_[level].empty()) mask |= (uint64_t(1) << level);
    }
    return mask;
}

}  // namespace ir
