
#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <tiffio.h>

#ifdef _WIN32
#include <windows.h>
static std::wstring TiffUtf8ToWstr(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
#endif

namespace TiffReader {

static bool Load16(const std::string& path, int& w, int& h, std::vector<uint16_t>& out) {
#ifdef _WIN32
    TIFF* tif = TIFFOpenW(TiffUtf8ToWstr(path).c_str(), "r");
#else
    TIFF* tif = TIFFOpen(path.c_str(), "r");
#endif
    if (!tif) {
        std::cerr << "[TiffReader] Failed to open: " << path << std::endl;
        return false;
    }

    uint32_t width = 0, height = 0;
    uint16_t bits_per_sample = 0, samples_per_pixel = 1; // Default to 1 channel if tag is missing

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    // TIFFGetField returns 1 on success, 0 on failure. For grayscale, SamplesPerPixel might not be set.
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);

    if (width == 0 || height == 0) {
        std::cerr << "[TiffReader] Invalid image dimensions." << std::endl;
        TIFFClose(tif);
        return false;
    }
    
    if (bits_per_sample != 16) {
        std::cerr << "[TiffReader] Unsupported bit depth: " << bits_per_sample << std::endl;
        TIFFClose(tif);
        return false;
    }
    
    if (samples_per_pixel != 1) {
        std::cerr << "[TiffReader] Unsupported channels: " << samples_per_pixel << std::endl;
        TIFFClose(tif);
        return false;
    }

    out.resize(width * height);
    
    tsize_t scanline_size = TIFFScanlineSize(tif);
    if (scanline_size < width * 2) {
        std::cerr << "[TiffReader] Unexpected scanline size: " << scanline_size << std::endl;
        TIFFClose(tif);
        return false;
    }
    
    std::vector<uint8_t> scanline_buf(scanline_size);
    for (uint32_t row = 0; row < height; ++row) {
        if (TIFFReadScanline(tif, scanline_buf.data(), row, 0) < 0) {
            std::cerr << "[TiffReader] Error reading scanline " << row << std::endl;
            TIFFClose(tif);
            return false;
        }
        memcpy(out.data() + row * width, scanline_buf.data(), width * 2);
    }

    w = width;
    h = height;

    TIFFClose(tif);
    return true;
}

} // namespace TiffReader
