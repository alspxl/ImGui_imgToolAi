#pragma once
// tiff_reader.h
// Minimal baseline/uncompressed TIFF reader for 16-bit grayscale images.
// Covers the most common flat-panel detector output format.
// Supports:
//   - TIFF 6.0 little-endian and big-endian byte order
//   - Compression: None (1) and PackBits (32773) — skipped for now, only None
//   - BitsPerSample: 16
//   - SamplesPerPixel: 1 (grayscale)
//   - Planar configuration: 1 (chunky, irrelevant for single channel)
//   - StripOffsets / StripByteCounts layout
// Returns false for anything it cannot handle so callers can fall back.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace TiffReader {

namespace detail {

// TIFF tag IDs we care about.
enum Tag : uint16_t {
    ImageWidth      = 256,
    ImageLength     = 257,
    BitsPerSample   = 258,
    Compression     = 259,
    PhotometricInterp = 262,
    StripOffsets    = 273,
    SamplesPerPixel = 277,
    RowsPerStrip    = 278,
    StripByteCounts = 279,
    PlanarConfig    = 284,
};

// TIFF field types.
enum Type : uint16_t {
    BYTE   = 1,
    ASCII  = 2,
    SHORT  = 3,
    LONG   = 4,
    RATIONAL = 5,
};

static inline uint16_t swap16(uint16_t v) {
    return static_cast<uint16_t>((v >> 8) | (v << 8));
}
static inline uint32_t swap32(uint32_t v) {
    return ((v & 0xFF000000u) >> 24) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x000000FFu) << 24);
}

struct IFDEntry {
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value_offset; // raw 4-byte value/offset field
};

// Read a typed IFD value (scalar; returns first element).
static uint32_t ReadValue(const IFDEntry& e, bool swap) {
    uint32_t raw = swap ? swap32(e.value_offset) : e.value_offset;
    uint16_t type = swap ? swap16(e.type) : e.type;
    if (type == SHORT) {
        // SHORT stores two values in the 4-byte field (LE: low word first).
        uint16_t lo = static_cast<uint16_t>(raw & 0xFFFF);
        return swap ? swap16(lo) : lo;
    }
    return raw; // LONG or BYTE
}

// Read an array of uint32 values from file (for multi-entry tags like StripOffsets).
static std::vector<uint32_t> ReadArray(std::ifstream& f,
                                       const IFDEntry& e,
                                       bool swap) {
    uint16_t type  = swap ? swap16(e.type)  : e.type;
    uint32_t count = swap ? swap32(e.count) : e.count;

    std::vector<uint32_t> result(count);

    // If count == 1 (or data fits in 4 bytes for SHORT <= 2), value is inline.
    size_t elem_size = (type == SHORT) ? 2 : 4;
    if (count * elem_size <= 4) {
        uint32_t raw = swap ? swap32(e.value_offset) : e.value_offset;
        for (uint32_t i = 0; i < count; ++i) {
            if (type == SHORT) {
                uint16_t v = static_cast<uint16_t>((raw >> (i * 16)) & 0xFFFF);
                result[i] = swap ? swap16(v) : v;
            } else {
                result[i] = raw;
            }
        }
        return result;
    }

    // Otherwise value_offset is the file offset to the array.
    uint32_t offset = swap ? swap32(e.value_offset) : e.value_offset;
    f.seekg(offset, std::ios::beg);
    for (uint32_t i = 0; i < count; ++i) {
        if (type == SHORT) {
            uint16_t v = 0;
            f.read(reinterpret_cast<char*>(&v), 2);
            result[i] = swap ? swap16(v) : v;
        } else {
            uint32_t v = 0;
            f.read(reinterpret_cast<char*>(&v), 4);
            result[i] = swap ? swap32(v) : v;
        }
    }
    return result;
}

} // namespace detail

// Load a 16-bit grayscale TIFF into a uint16_t vector.
// Returns true on success; w/h/out are only modified on success.
static bool Load16(const std::string& path,
                   int& w, int& h,
                   std::vector<uint16_t>& out) {
    using namespace detail;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    // Read TIFF header (8 bytes).
    uint8_t hdr[8];
    f.read(reinterpret_cast<char*>(hdr), 8);
    if (f.gcount() != 8) return false;

    bool big_endian = false;
    if (hdr[0] == 'M' && hdr[1] == 'M') {
        big_endian = true;
    } else if (hdr[0] == 'I' && hdr[1] == 'I') {
        big_endian = false;
    } else {
        return false; // Not a TIFF
    }
    bool swap = big_endian; // whether we need to byte-swap values

    uint16_t magic;
    std::memcpy(&magic, hdr + 2, 2);
    if (swap) magic = swap16(magic);
    if (magic != 42) return false; // TIFF magic number

    uint32_t ifd_offset;
    std::memcpy(&ifd_offset, hdr + 4, 4);
    if (swap) ifd_offset = swap32(ifd_offset);

    // Read first IFD.
    f.seekg(ifd_offset, std::ios::beg);
    uint16_t num_entries = 0;
    f.read(reinterpret_cast<char*>(&num_entries), 2);
    if (swap) num_entries = swap16(num_entries);
    if (num_entries == 0 || num_entries > 4096) return false;

    std::vector<IFDEntry> entries(num_entries);
    for (int i = 0; i < num_entries; ++i) {
        f.read(reinterpret_cast<char*>(&entries[i]), 12);
        if (f.gcount() != 12) return false;
    }

    // Parse required tags.
    uint32_t img_w = 0, img_h = 0;
    uint32_t bits   = 8;
    uint32_t spp    = 1;
    uint32_t compr  = 1;
    std::vector<uint32_t> strip_offsets;
    std::vector<uint32_t> strip_bytecounts;

    for (auto& e : entries) {
        uint16_t tag = swap ? swap16(e.tag) : e.tag;
        switch (tag) {
            case ImageWidth:       img_w  = ReadValue(e, swap); break;
            case ImageLength:      img_h  = ReadValue(e, swap); break;
            case BitsPerSample:    bits   = ReadValue(e, swap); break;
            case Compression:      compr  = ReadValue(e, swap); break;
            case SamplesPerPixel:  spp    = ReadValue(e, swap); break;
            case StripOffsets:     strip_offsets     = ReadArray(f, e, swap); break;
            case StripByteCounts:  strip_bytecounts  = ReadArray(f, e, swap); break;
            default: break;
        }
    }

    // Validate.
    if (img_w == 0 || img_h == 0)            return false;
    if (bits != 16)                           return false;
    if (spp  != 1)                            return false;
    if (compr != 1)                           return false; // only uncompressed
    if (strip_offsets.empty())                return false;
    if (strip_offsets.size() != strip_bytecounts.size()) return false;

    // Verify total byte count matches expected.
    size_t total_bytes = static_cast<size_t>(img_w) * img_h * 2;
    size_t sum_bytes   = 0;
    for (auto bc : strip_bytecounts) sum_bytes += bc;
    if (sum_bytes < total_bytes) return false;

    // Read strips.
    std::vector<uint16_t> buf(static_cast<size_t>(img_w) * img_h);
    size_t written = 0;
    for (size_t si = 0; si < strip_offsets.size(); ++si) {
        f.seekg(strip_offsets[si], std::ios::beg);
        size_t bc = strip_bytecounts[si];
        size_t pix = bc / 2;
        if (written + pix > buf.size()) pix = buf.size() - written;
        f.read(reinterpret_cast<char*>(buf.data() + written), static_cast<std::streamsize>(pix * 2));
        written += pix;
        if (written >= buf.size()) break;
    }
    if (written < buf.size()) return false;

    // Byte-swap pixels if big-endian TIFF.
    if (big_endian) {
        for (auto& v : buf) v = swap16(v);
    }

    w   = static_cast<int>(img_w);
    h   = static_cast<int>(img_h);
    out = std::move(buf);
    return true;
}

} // namespace TiffReader
