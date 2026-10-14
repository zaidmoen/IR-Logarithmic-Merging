// merge.hpp — merging on-disk indexes (IIR §4.2–4.3).
// PROVIDED (src/merge.cpp). You do not need to change this file.
#pragma once

#include <string>
#include <vector>

#include "ir/common.hpp"
#include "ir/index_io.hpp"

namespace ir {

// ---------------------------------------------------------------------------
// k-way merge.
// Merges the indexes `inputs` into ONE index at `out_base`, reading each input
// sequentially with an IndexCursor and holding at most one postings list per
// input in memory. Use a priority queue (min-heap) on the current terms.
// When several inputs contain the same term, the merged list must be sorted
// and free of duplicates. The inputs are NOT deleted.
// Both dynamic indexes call this function for every merge they perform.
void merge_indexes(const std::vector<std::string>& inputs, const std::string& out_base);

}  // namespace ir
