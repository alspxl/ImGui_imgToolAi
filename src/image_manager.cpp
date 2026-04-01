// image_manager.cpp
// Implementation of image loading, saving, and OpenGL texture management.
// All pixel buffers are normalised to new[]-allocated memory.

#include "image_manager.h"

// Include stb headers (implementation is in stb_impl.cpp).
#include "stb_image.h"
#include "stb_image_write.h"

// OpenGL
#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
#else
  #include <GL/gl.h>
#endif

#include <cstring>

ImageManager::ImageManager()  = default;
ImageManager::~ImageManager() {
    FreeTexture();
    FreePixels();
}

void ImageManager::FreePixels() {
    // pixels_ is always new[]-allocated after LoadImage normalises stb memory.
    delete[] pixels_;
    pixels_   = nullptr;
    width_    = 0;
    height_   = 0;
    channels_ = 0;
}

bool ImageManager::LoadImage(const std::string& path) {
    FreeTexture();
    FreePixels();

    // Always load as RGBA (4 channels) for uniform handling.
    int w = 0, h = 0, c = 0;
    unsigned char* stb_data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!stb_data) return false;

    // Normalise to new[] so all code paths use the same allocator.
    std::size_t size = static_cast<std::size_t>(w) * h * 4;
    pixels_ = new unsigned char[size];
    std::memcpy(pixels_, stb_data, size);
    stbi_image_free(stb_data);

    width_     = w;
    height_    = h;
    channels_  = 4;
    file_path_ = path;

    UploadTexture();
    return true;
}

bool ImageManager::SaveImage(const std::string& path, const std::string& format) const {
    if (!pixels_) return false;

    int result = 0;
    if (format == "png") {
        result = stbi_write_png(path.c_str(), width_, height_, channels_,
                                pixels_, width_ * channels_);
    } else if (format == "bmp") {
        result = stbi_write_bmp(path.c_str(), width_, height_, channels_, pixels_);
    } else if (format == "jpg" || format == "jpeg") {
        result = stbi_write_jpg(path.c_str(), width_, height_, channels_, pixels_, 90);
    } else if (format == "tga") {
        result = stbi_write_tga(path.c_str(), width_, height_, channels_, pixels_);
    }
    return result != 0;
}

void ImageManager::UploadTexture() {
    if (!pixels_) return;

    if (texture_id_ == 0)
        glGenTextures(1, &texture_id_);

    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum fmt = (channels_ == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, fmt, GL_UNSIGNED_BYTE, pixels_);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ImageManager::FreeTexture() {
    if (texture_id_) {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
}

void ImageManager::SetPixels(unsigned char* data, int width, int height, int channels) {
    FreePixels();
    pixels_   = data;   // takes ownership (caller must have used new[])
    width_    = width;
    height_   = height;
    channels_ = channels;
    UploadTexture();
}

unsigned char* ImageManager::ClonePixels() const {
    if (!pixels_) return nullptr;
    std::size_t size = static_cast<std::size_t>(width_) * height_ * channels_;
    auto* copy = new unsigned char[size];
    std::memcpy(copy, pixels_, size);
    return copy;
}
