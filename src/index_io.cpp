// index_io.cpp — PROVIDED implementation of the on-disk index format.
#include "ir/index_io.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace ir {

IOStats& io_stats() {
    static IOStats s;
    return s;
}

// ---------------------------------------------------------------- writer
IndexWriter::IndexWriter(const std::string& base)
    : dict_(base + ".dict"), post_(base + ".post", std::ios::binary) {
    if (!dict_ || !post_) throw std::runtime_error("cannot create index " + base);
    open_ = true;
    io_stats().indexes_created++;
}

IndexWriter::~IndexWriter() {
    if (open_) close();
}

void IndexWriter::add(const std::string& term, const PostingsList& postings) {
    if (!open_) throw std::logic_error("IndexWriter::add after close");
    if (postings.empty()) throw std::logic_error("empty postings list for " + term);
    if (nterms_ > 0 && term <= last_term_)
        throw std::logic_error("terms out of order: '" + last_term_ + "' then '" + term + "'");
    std::string bytes = vb_encode_postings(postings);
    post_.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    dict_ << term << '\t' << postings.size() << '\t' << offset_ << '\t' << bytes.size() << '\n';
    offset_ += bytes.size();
    last_term_ = term;
    nterms_++;
    io_stats().postings_written += postings.size();
    io_stats().bytes_written += bytes.size();
}

void IndexWriter::close() {
    if (!open_) return;
    dict_.close();
    post_.close();
    open_ = false;
}

// ---------------------------------------------------------------- helpers
static bool parse_dict_line(const std::string& line, DictEntry& e) {
    if (line.empty()) return false;
    std::istringstream ss(line);
    std::string df, off, nb;
    if (!std::getline(ss, e.term, '\t') || !std::getline(ss, df, '\t') ||
        !std::getline(ss, off, '\t') || !std::getline(ss, nb))
        throw std::runtime_error("malformed dictionary line: " + line);
    e.df = static_cast<std::uint32_t>(std::stoul(df));
    e.offset = std::stoull(off);
    e.nbytes = std::stoull(nb);
    return true;
}

static void read_postings(std::ifstream& post, const DictEntry& e, PostingsList& out) {
    std::string bytes(e.nbytes, '\0');
    post.seekg(static_cast<std::streamoff>(e.offset));
    post.read(bytes.data(), static_cast<std::streamsize>(e.nbytes));
    if (!post) throw std::runtime_error("short read in postings file");
    vb_decode_postings(bytes, out);
    if (out.size() != e.df) throw std::runtime_error("df mismatch for term " + e.term);
    io_stats().postings_read += out.size();
    io_stats().bytes_read += e.nbytes;
}

// ---------------------------------------------------------------- cursor
IndexCursor::IndexCursor(const std::string& base)
    : dict_(base + ".dict"), post_(base + ".post", std::ios::binary) {
    if (!dict_ || !post_) throw std::runtime_error("cannot open index " + base);
    next();
}

void IndexCursor::next() {
    std::string line;
    valid_ = false;
    while (std::getline(dict_, line)) {
        if (parse_dict_line(line, entry_)) {
            read_postings(post_, entry_, postings_);
            valid_ = true;
            return;
        }
    }
    postings_.clear();
}

// ---------------------------------------------------------------- reader
IndexReader::IndexReader(const std::string& base)
    : base_(base), post_(base + ".post", std::ios::binary) {
    std::ifstream dict(base + ".dict");
    if (!dict || !post_) throw std::runtime_error("cannot open index " + base);
    std::string line;
    DictEntry e;
    while (std::getline(dict, line))
        if (parse_dict_line(line, e)) dict_.push_back(e);
}

bool IndexReader::get(const std::string& term, PostingsList& out) const {
    auto it = std::lower_bound(dict_.begin(), dict_.end(), term,
                               [](const DictEntry& e, const std::string& t) { return e.term < t; });
    out.clear();
    if (it == dict_.end() || it->term != term) return false;
    read_postings(post_, *it, out);
    return true;
}

// ---------------------------------------------------------------- files
bool index_exists(const std::string& base) {
    return fs::exists(base + ".dict") && fs::exists(base + ".post");
}

void remove_index(const std::string& base) {
    std::error_code ec;
    fs::remove(base + ".dict", ec);
    fs::remove(base + ".post", ec);
}

void rename_index(const std::string& from, const std::string& to) {
    remove_index(to);
    fs::rename(from + ".dict", to + ".dict");
    fs::rename(from + ".post", to + ".post");
}

std::uint64_t index_size_bytes(const std::string& base) {
    return fs::file_size(base + ".dict") + fs::file_size(base + ".post");
}

// ---------------------------------------------------------------- corpus
CorpusReader::CorpusReader(const std::string& path) : in_(path) {
    if (!in_) throw std::runtime_error("cannot open corpus " + path);
}

bool CorpusReader::next(DocId& id, std::string& text) {
    if (!std::getline(in_, text)) return false;
    id = next_id_++;
    return true;
}

}  // namespace ir
