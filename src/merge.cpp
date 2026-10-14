// merge.cpp — PROVIDED. k-way merge of on-disk indexes with a min-heap.
#include "ir/merge.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <queue>

namespace ir {

void merge_indexes(const std::vector<std::string>& inputs, const std::string& out_base) {
    std::vector<std::unique_ptr<IndexCursor>> cur;
    cur.reserve(inputs.size());
    for (const std::string& in : inputs) cur.push_back(std::make_unique<IndexCursor>(in));

    // min-heap on (term, input number)
    using Item = std::pair<std::string, std::size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
    for (std::size_t i = 0; i < cur.size(); ++i)
        if (cur[i]->valid()) heap.push({cur[i]->term(), i});

    IndexWriter out(out_base);
    PostingsList merged, tmp;
    while (!heap.empty()) {
        const std::string term = heap.top().first;
        merged.clear();
        // pop every input whose current term is `term`
        while (!heap.empty() && heap.top().first == term) {
            std::size_t i = heap.top().second;
            heap.pop();
            const PostingsList& p = cur[i]->postings();
            if (merged.empty() || merged.back() < p.front()) {
                merged.insert(merged.end(), p.begin(), p.end());     // disjoint, in order
            } else {                                                 // overlapping ranges
                tmp.clear();
                std::set_union(merged.begin(), merged.end(), p.begin(), p.end(),
                               std::back_inserter(tmp));
                merged.swap(tmp);
            }
            cur[i]->next();
            if (cur[i]->valid()) heap.push({cur[i]->term(), i});
        }
        out.add(term, merged);
    }
    out.close();
}

}  // namespace ir
