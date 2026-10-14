// index_io.hpp — on-disk inverted index format, I/O counters, corpus reader.
// PROVIDED: you do not need to change this file.
//
// An on-disk index (a flushed auxiliary index, a logarithmic-merging
// generation, or the final index) is a pair of files sharing a base path B:
//
//   B.dict  text, one line per term, terms in increasing byte order:
//           term <TAB> df <TAB> offset <TAB> nbytes
//   B.post  binary, the variable-byte encoded gap lists, back to back,
//           in the same order as B.dict
//
// Every IndexWriter / IndexCursor / IndexReader updates the global IOStats,
// so your report numbers come from here and are comparable across students.
#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "ir/common.hpp"

namespace ir {

struct IOStats {
    std::uint64_t postings_written = 0;   // docIDs written to any .post file
    std::uint64_t postings_read = 0;      // docIDs read back from any .post file
    std::uint64_t bytes_written = 0;      // bytes written to .post files
    std::uint64_t bytes_read = 0;         // bytes read from .post files
    std::uint64_t indexes_created = 0;    // number of IndexWriter objects opened
    void reset() { *this = IOStats{}; }
};
IOStats& io_stats();                       // the global counters

struct DictEntry {
    std::string term;
    std::uint32_t df = 0;
    std::uint64_t offset = 0;
    std::uint64_t nbytes = 0;
};

// Writes one index. Terms must be added in strictly increasing order and
// every postings list must be non-empty and strictly increasing.
class IndexWriter {
public:
    explicit IndexWriter(const std::string& base);
    ~IndexWriter();
    IndexWriter(const IndexWriter&) = delete;
    IndexWriter& operator=(const IndexWriter&) = delete;

    void add(const std::string& term, const PostingsList& postings);
    void close();
    std::uint64_t terms_written() const { return nterms_; }

private:
    std::ofstream dict_, post_;
    std::string last_term_;
    std::uint64_t offset_ = 0, nterms_ = 0;
    bool open_ = false;
};

// Sequential scan of an index in term order. Holds ONE postings list in
// memory at a time, so k cursors can be merged with O(k) lists in memory.
class IndexCursor {
public:
    explicit IndexCursor(const std::string& base);
    bool valid() const { return valid_; }
    const std::string& term() const { return entry_.term; }
    std::uint32_t df() const { return entry_.df; }
    const PostingsList& postings() const { return postings_; }
    void next();                           // advance to the next term

private:
    std::ifstream dict_, post_;
    DictEntry entry_;
    PostingsList postings_;
    bool valid_ = false;
};

// Random access: loads the dictionary into memory, reads postings on demand.
class IndexReader {
public:
    explicit IndexReader(const std::string& base);
    bool get(const std::string& term, PostingsList& out) const;
    const std::vector<DictEntry>& dictionary() const { return dict_; }

private:
    std::string base_;
    std::vector<DictEntry> dict_;
    mutable std::ifstream post_;
};

bool index_exists(const std::string& base);
void remove_index(const std::string& base);                 // deletes B.dict, B.post
void rename_index(const std::string& from, const std::string& to);
std::uint64_t index_size_bytes(const std::string& base);    // size of both files

// Reads a collection with one document per line; docID = line number (1-based).
class CorpusReader {
public:
    explicit CorpusReader(const std::string& path);
    bool next(DocId& id, std::string& text);

private:
    std::ifstream in_;
    DocId next_id_ = 1;
};

}  // namespace ir
