#include "os_dialogs.h"

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <string.h>

std::string OS_OpenFileDialog(const char* filter) {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    return GetOpenFileNameA(&ofn) ? std::string(filename) : std::string();
}

std::string OS_SaveFileDialog(const char* filter) {
    char filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    return GetSaveFileNameA(&ofn) ? std::string(filename) : std::string();
}
#else
// Fallback stub for non-Windows
std::string OS_OpenFileDialog(const char* filter) { return ""; }
std::string OS_SaveFileDialog(const char* filter) { return ""; }
#endif