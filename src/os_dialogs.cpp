#include "os_dialogs.h"
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <string.h>

std::string OS_OpenFileDialog(const char* filter) {
    wchar_t filename[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    
    std::vector<wchar_t> wfilter;
    if (filter) {
        int len = 0;
        while(filter[len] != '\0' || filter[len+1] != '\0') { len++; }
        len += 2;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, filter, len, NULL, 0);
        wfilter.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, filter, len, wfilter.data(), wlen);
        ofn.lpstrFilter = wfilter.data();
    } else {
        ofn.lpstrFilter = L"All Files\0*.*\0";
    }

    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    
    if (GetOpenFileNameW(&ofn)) {
        int u8len = WideCharToMultiByte(CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL);
        std::string res(u8len, 0);
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, &res[0], u8len, NULL, NULL);
        if(!res.empty() && res.back() == '\0') res.pop_back(); // Remove null terminator for std::string
        res.resize(strlen(res.c_str()));
        return res;
    }
    return std::string();
}

std::string OS_SaveFileDialog(const char* filter) {
    wchar_t filename[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    
    std::vector<wchar_t> wfilter;
    if (filter) {
        int len = 0;
        while(filter[len] != '\0' || filter[len+1] != '\0') { len++; }
        len += 2;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, filter, len, NULL, 0);
        wfilter.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, filter, len, wfilter.data(), wlen);
        ofn.lpstrFilter = wfilter.data();
    } else {
        ofn.lpstrFilter = L"All Files\0*.*\0";
    }

    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    
    if (GetSaveFileNameW(&ofn)) {
        int u8len = WideCharToMultiByte(CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL);
        std::string res(u8len, 0);
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, &res[0], u8len, NULL, NULL);
        if(!res.empty() && res.back() == '\0') res.pop_back(); // Remove null terminator for std::string
        res.resize(strlen(res.c_str()));
        return res;
    }
    return std::string();
}
#else
// Fallback stub for non-Windows
std::string OS_OpenFileDialog(const char* filter) { return ""; }
std::string OS_SaveFileDialog(const char* filter) { return ""; }
#endif