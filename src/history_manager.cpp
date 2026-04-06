// history_manager.cpp
// Implementation of the undo/redo history manager.

#include "history_manager.h"
#include <cstring>
#include <algorithm>

HistoryManager::HistoryManager() = default;
HistoryManager::~HistoryManager() = default;

void HistoryManager::PushState(int width, int height, int channels, const unsigned char* data) {
    State s;
    s.width    = width;
    s.height   = height;
    s.channels = channels;
    std::size_t size = static_cast<std::size_t>(width) * height * channels;
    s.data.assign(data, data + size);

    undo_stack_.push_back(std::move(s));

    // Trim oldest entries if over the limit.
    while (static_cast<int>(undo_stack_.size()) > kMaxHistory) {
        undo_stack_.erase(undo_stack_.begin());
    }

    // Any new operation invalidates the redo stack.
    redo_stack_.clear();
}

bool HistoryManager::Undo(int cur_width, int cur_height, int cur_channels, const unsigned char* cur_data,
                          int& out_width, int& out_height, int& out_channels, unsigned char*& out_data) {
    if (undo_stack_.empty()) return false;

    // Save the currently-displayed image onto the redo stack so that Redo can
    // bring it back.
    {
        State cur;
        cur.width    = cur_width;
        cur.height   = cur_height;
        cur.channels = cur_channels;
        std::size_t size = static_cast<std::size_t>(cur_width) * cur_height * cur_channels;
        cur.data.assign(cur_data, cur_data + size);
        redo_stack_.push_back(std::move(cur));
        // Trim redo stack to keep memory bounded.
        while (static_cast<int>(redo_stack_.size()) > kMaxHistory)
            redo_stack_.erase(redo_stack_.begin());
    }

    State s = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    out_width    = s.width;
    out_height   = s.height;
    out_channels = s.channels;

    std::size_t size = static_cast<std::size_t>(s.width) * s.height * s.channels;
    out_data = new unsigned char[size];
    std::memcpy(out_data, s.data.data(), size);

    return true;
}

bool HistoryManager::Redo(int cur_width, int cur_height, int cur_channels, const unsigned char* cur_data,
                          int& out_width, int& out_height, int& out_channels, unsigned char*& out_data) {
    if (redo_stack_.empty()) return false;

    // Save the currently-displayed image onto the undo stack so that Undo can
    // bring it back.
    {
        State cur;
        cur.width    = cur_width;
        cur.height   = cur_height;
        cur.channels = cur_channels;
        std::size_t size = static_cast<std::size_t>(cur_width) * cur_height * cur_channels;
        cur.data.assign(cur_data, cur_data + size);
        undo_stack_.push_back(std::move(cur));
        // Trim undo stack.
        while (static_cast<int>(undo_stack_.size()) > kMaxHistory)
            undo_stack_.erase(undo_stack_.begin());
    }

    State s = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    out_width    = s.width;
    out_height   = s.height;
    out_channels = s.channels;

    std::size_t size = static_cast<std::size_t>(s.width) * s.height * s.channels;
    out_data = new unsigned char[size];
    std::memcpy(out_data, s.data.data(), size);

    return true;
}

bool HistoryManager::CanUndo() const { return !undo_stack_.empty(); }
bool HistoryManager::CanRedo() const { return !redo_stack_.empty(); }

void HistoryManager::Clear() {
    undo_stack_.clear();
    redo_stack_.clear();
}
