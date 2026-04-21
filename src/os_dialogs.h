#pragma once

#include <string>

// Opens a native file dialog for opening a file.
// Returns the selected file path, or an empty string if cancelled.
std::string OS_OpenFileDialog(const char* filter);

// Opens a native file dialog for saving a file.
// Returns the selected file path, or an empty string if cancelled.
std::string OS_SaveFileDialog(const char* filter);