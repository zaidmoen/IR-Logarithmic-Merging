// common.hpp — basic types, tokenizer and variable-byte codec.
// PROVIDED: you do not need to change this file.
#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace ir {

using DocId = std::uint32_t;
using PostingsList = std::vector<DocId>;   // strictly increasing docIDs

// Lower-cases ASCII letters and splits on every non-alphanumeric character.
// "Brutus killed Caesar." -> {"brutus", "killed", "caesar"}
inline void tokenize(const std::string& text, std::vector<std::string>& out) {
    out.clear();
    std::string cur;
    for (unsigned char c : text) {
        if (std::isalnum(c)) {
            cur.push_back(static_cast<char>(std::tolower(c)));
        } else if (!cur.empty()) {
            out.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(cur);
}

// ---- Variable-byte encoding of docID gaps (IIR §5.3.1) -------------------
// Each number is written as 7-bit groups, most significant first; the last
// byte of a number has its high bit set.

inline void vb_encode_number(std::uint32_t n, std::string& out) {
    unsigned char buf[5];
    int len = 0;
    do {
        buf[len++] = static_cast<unsigned char>(n % 128);
        n /= 128;
    } while (n > 0);
    buf[0] |= 0x80;                         // terminator bit on the last byte
    for (int i = len - 1; i >= 0; --i) out.push_back(static_cast<char>(buf[i]));
}

// Encodes a postings list as gaps: 3, 7, 12 -> 3, 4, 5
inline std::string vb_encode_postings(const PostingsList& p) {
    std::string out;
    DocId prev = 0;
    for (DocId d : p) {
        if (d <= prev && !(prev == 0 && d > 0))
            throw std::logic_error("postings must be strictly increasing and > 0");
        vb_encode_number(d - prev, out);
        prev = d;
    }
    return out;
}

inline void vb_decode_postings(const std::string& bytes, PostingsList& out) {
    out.clear();
    std::uint32_t n = 0;
    DocId prev = 0;
    for (unsigned char b : bytes) {
        if (b < 128) {
            n = n * 128 + b;
        } else {
            n = n * 128 + (b - 128);
            prev += n;
            out.push_back(prev);
            n = 0;
        }
    }
}

}  // namespace ir
