// aux_main.cpp — PROVIDED. Auxiliary + main index (IIR §4.5), the baseline
// you compare logarithmic merging against. Every flush merges Z0 into the
// single main index, rewriting all of it.
#include "ir/dynamic.hpp"

namespace ir {

AuxMainIndex::~AuxMainIndex() {
    if (!main_.empty()) remove_index(main_);
}

void AuxMainIndex::flush() {
    if (z0_.empty()) return;
    std::string z = new_temp_name();
    z0_.write_to(z);
    z0_.clear();
    if (main_.empty()) {               // first flush: Z0 becomes the main index
        main_ = z;
        return;
    }
    std::string merged = new_temp_name();
    merge_indexes({main_, z}, merged); // rewrites the WHOLE main index
    stats_.merges++;
    readers_.forget(main_);
    remove_index(main_);
    remove_index(z);
    main_ = merged;
}

PostingsList AuxMainIndex::lookup(const std::string& term) const {
    return union_postings(disk_lookup(readers_, main_, term), z0_.lookup(term));
}

void AuxMainIndex::finalize(const std::string& out_base) {
    flush();
    if (main_.empty()) {
        write_empty_index(out_base);
        return;
    }
    readers_.forget(main_);
    rename_index(main_, out_base);
    main_.clear();
}

std::size_t AuxMainIndex::disk_indexes() const { return main_.empty() ? 0 : 1; }

}  // namespace ir
