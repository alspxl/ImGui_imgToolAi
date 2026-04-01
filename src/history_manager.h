#pragma once
// history_manager.h
// Manages undo/redo history for image editing operations.
// Each state stores a full copy of the pixel buffer.

#include <vector>
#include <cstdint>

class HistoryManager {
public:
    // Maximum number of undo/redo steps stored.
    static constexpr int kMaxHistory = 20;

    HistoryManager();
    ~HistoryManager();

    // Push the current image state onto the undo stack.
    // width, height, channels describe the image dimensions.
    // data points to the pixel buffer (width * height * channels bytes).
    void PushState(int width, int height, int channels, const unsigned char* data);

    // Undo: restore the previous state.
    // Returns true and fills out_width/out_height/out_channels/out_data on success.
    // The caller takes ownership of out_data (must be freed with delete[]).
    bool Undo(int& out_width, int& out_height, int& out_channels, unsigned char*& out_data);

    // Redo: re-apply the previously undone state.
    // Returns true and fills out_width/out_height/out_channels/out_data on success.
    // The caller takes ownership of out_data (must be freed with delete[]).
    bool Redo(int& out_width, int& out_height, int& out_channels, unsigned char*& out_data);

    bool CanUndo() const;
    bool CanRedo() const;

    // Clear all history.
    void Clear();

private:
    struct State {
        int width;
        int height;
        int channels;
        std::vector<unsigned char> data;
    };

    std::vector<State> undo_stack_;
    std::vector<State> redo_stack_;
};
