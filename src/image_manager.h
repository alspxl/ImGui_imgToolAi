#pragma once
// image_manager.h
// Manages image loading, saving, and OpenGL texture upload/release.

#include <string>
#include <cstdint>

// Forward-declare so callers don't need GL headers directly.
typedef unsigned int GLuint;

class ImageManager {
public:
    ImageManager();
    ~ImageManager();

    // Load an image from disk.  Converts any channel count to RGBA (4 channels).
    // Returns true on success.
    bool LoadImageFile(const std::string& path);

    // Save the current image to disk.
    // format: "png", "jpg", "bmp", "tga"
    bool SaveImage(const std::string& path, const std::string& format) const;

    // Upload the current pixel buffer to an OpenGL texture.
    // Creates a new texture if one does not already exist; updates it otherwise.
    void UploadTexture();

    // Upload an external buffer to the existing texture without modifying the original data.
    void UpdateTextureFrom(const unsigned char* data);

    // Free the OpenGL texture.
    void FreeTexture();

    // Replace the pixel buffer with externally-managed data (takes ownership).
    void SetPixels(unsigned char* data, int width, int height, int channels);

    // Accessors.
    unsigned char* GetPixels()   const { return pixels_; }
    int            GetWidth()    const { return width_; }
    int            GetHeight()   const { return height_; }
    int            GetChannels() const { return channels_; }
    GLuint         GetTexture()  const { return texture_id_; }
    bool           HasImage()    const { return pixels_ != nullptr; }
    const std::string& GetFilePath() const { return file_path_; }

    // Get a copy of the current pixel buffer (caller must delete[]).
    unsigned char* ClonePixels() const;

private:
    unsigned char* pixels_     = nullptr;
    int            width_      = 0;
    int            height_     = 0;
    int            channels_   = 0;
    GLuint         texture_id_ = 0;
    std::string    file_path_;

    // Free the pixel buffer (always allocated with new[]).
    void FreePixels();
};
