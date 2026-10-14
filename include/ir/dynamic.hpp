// dynamic.hpp — dynamic indexing with an auxiliary index (IIR §4.5).
// PROVIDED interface. You implement the classes in src/dynamic.cpp.
//
// Documents arrive one at a time. New postings go into an in-memory
// auxiliary index Z0 that may hold at most `n` postings. When adding a
// document would overflow Z0, Z0 is flushed to disk according to the policy
// of the concrete class. At ANY moment, lookup() must return the complete,
// sorted postings list over all documents added so far.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ir/common.hpp"
#include "ir/memory_index.hpp"
#include "ir/merge.hpp"

namespace ir {

struct DynamicStats {
    std::uint64_t flushes = 0;          // times Z0 was written out
    std::uint64_t merges = 0;           // calls to merge_indexes
    std::uint64_t max_disk_indexes = 0; // largest number of on-disk indexes alive at once
};

// PROVIDED. Keeps an IndexReader (and so the dictionary) open per on-disk
// index, so repeated lookups do not reload the dictionary every time.
// Call forget(base) BEFORE you delete or rename an index.
class ReaderCache {
public:
    const IndexReader& get(const std::string& base) const;
    void forget(const std::string& base);

private:
    mutable std::map<std::string, std::unique_ptr<IndexReader>> open_;
};

class DynamicIndex {
public:
    DynamicIndex(std::string work_dir, std::size_t n) : work_dir_(std::move(work_dir)), n_(n) {}
    virtual ~DynamicIndex() = default;

    // Adds a document (docIDs increasing). Flushes Z0 first if needed.
    void add_document(DocId id, const std::vector<std::string>& tokens);

    // Complete postings list of `term` over all documents added so far.
    virtual PostingsList lookup(const std::string& term) const = 0;

    // Flushes Z0 and merges everything into ONE index at `out_base`.
    virtual void finalize(const std::string& out_base) = 0;

    // Number of on-disk indexes that currently exist (not counting Z0).
    virtual std::size_t disk_indexes() const = 0;

    const DynamicStats& stats() const { return stats_; }

protected:
    // Writes Z0 to disk according to the policy, then clears Z0.
    virtual void flush() = 0;

    std::string work_dir_;
    std::size_t n_;
    MemoryIndex z0_;          // the in-memory auxiliary index
    DynamicStats stats_;
    ReaderCache readers_;     // open readers for lookup()
    std::uint64_t next_file_ = 0;
    std::string new_temp_name();   // unique base path inside work_dir_
};

// ---------------------------------------------------------------------------
// PROVIDED (src/aux_main.cpp) — Auxiliary + main index, the baseline of
// IIR §4.5, for comparison in the report. Read it as an example: it uses the
// same helpers your LogarithmicIndex will need. One main index on disk. Every flush merges Z0 into the main index, which
// costs O(T^2 / n) postings written over the whole collection.
class AuxMainIndex : public DynamicIndex {
public:
    using DynamicIndex::DynamicIndex;
    ~AuxMainIndex() override;
    PostingsList lookup(const std::string& term) const override;
    void finalize(const std::string& out_base) override;
    std::size_t disk_indexes() const override;

protected:
    void flush() override;

private:
    std::string main_;        // base path of the main index ("" if none yet)
};

// ---------------------------------------------------------------------------
// YOUR TASK (src/logarithmic.cpp) — Logarithmic merging (IIR Figure 4.7,
// LMERGEADDTOKEN).
// Keeps indexes I0, I1, I2, ... on disk where Ii holds 2^i * n postings at
// most. A flush behaves like incrementing a binary counter:
//     Z = Z0 written to disk
//     for i = 0, 1, 2, ...
//         if Ii exists:  Z = merge(Ii, Z); delete Ii      (carry)
//         else:          Ii = Z; stop
// Total cost O(T log(T/n)) postings written; a lookup must consult Z0 and
// every existing Ii and combine the results.
class LogarithmicIndex : public DynamicIndex {
public:
    using DynamicIndex::DynamicIndex;
    ~LogarithmicIndex() override;
    PostingsList lookup(const std::string& term) const override;
    void finalize(const std::string& out_base) override;
    std::size_t disk_indexes() const override;

    // Bit i is 1 iff generation Ii currently exists (for tests and the report).
    std::uint64_t generation_mask() const;

protected:
    void flush() override;

private:
    std::vector<std::string> gen_;   // gen_[i] = base path of Ii, or "" if absent
};

// PROVIDED helpers (src/dynamic_base.cpp).
// Sorted union of two lists.
PostingsList union_postings(const PostingsList& a, const PostingsList& b);
// Postings of `term` in the on-disk index `base` ("" gives an empty list).
PostingsList disk_lookup(const ReaderCache& readers, const std::string& base,
                         const std::string& term);
// Writes an index with no terms (for finalize on an empty collection).
void write_empty_index(const std::string& base);

}  // namespace ir
