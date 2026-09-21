#include <windows.h>
#include <commdlg.h>      // OPENFILENAMEA, GetOpenFileNameA, OFN_* 宏
#include <dwmapi.h>
#include <shlobj.h>
#include <shobjidl.h>     // IFileOpenDialog, CLSID_FileOpenDialog
#include <objbase.h>      // CoInitializeEx, CoCreateInstance
#include <string.h>
#include <stdio.h>
#include <stdlib.h>       // malloc, free
#include <wchar.h>        // WideCharToMultiByte
#include "settings.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

// 去掉窗口圆角（Windows 11 有效）
void DisableWindowRoundedCorners(void* hwnd) {
    DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute((HWND)hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}

// 弹出"打开文件"对话框，只显示字体文件
// 返回：1 = 用户选了文件（路径写入 outPath），0 = 用户取消
int OpenFontFileDialog(char* outPath, int outPathSize) {
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter =
        "Font Files (*.ttf;*.otf;*.ttc)\0*.ttf;*.otf;*.ttc\0"
        "TrueType (*.ttf)\0*.ttf\0"
        "OpenType (*.otf)\0*.otf\0"
        "TrueType Collection (*.ttc)\0*.ttc\0"
        "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outPathSize;
    ofn.lpstrTitle = "Select Font File";
    ofn.lpstrInitialDir = "C:\\Windows\\Fonts";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    outPath[0] = '\0';
    return GetOpenFileNameA(&ofn) ? 1 : 0;
}

// 弹出"选择文件夹"对话框（资源管理器风格）
// 返回：1 = 用户选了目录（路径写入 outPath），0 = 用户取消
int OpenFolderDialog(void* hwndOwner, char* outPath, int outPathSize) {
    HRESULT hr = CoInitializeEx(NULL,
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    int needUninit = SUCCEEDED(hr);

    IFileOpenDialog* pDlg = NULL;
    hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        &IID_IFileOpenDialog, (void**)&pDlg);
    if (FAILED(hr)) {
        if (needUninit) CoUninitialize();
        outPath[0] = '\0';
        return 0;
    }

    DWORD options = 0;
    pDlg->lpVtbl->GetOptions(pDlg, &options);
    pDlg->lpVtbl->SetOptions(pDlg,
        options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    pDlg->lpVtbl->SetTitle(pDlg, L"Select Font Folder");

    hr = pDlg->lpVtbl->Show(pDlg, (HWND)hwndOwner);

    int result = 0;
    if (SUCCEEDED(hr)) {
        IShellItem* pItem = NULL;
        hr = pDlg->lpVtbl->GetResult(pDlg, &pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszPath = NULL;
            hr = pItem->lpVtbl->GetDisplayName(pItem, SIGDN_FILESYSPATH, &pszPath);
            if (SUCCEEDED(hr)) {
                WideCharToMultiByte(CP_ACP, 0, pszPath, -1,
                    outPath, outPathSize, NULL, NULL);
                result = 1;
                CoTaskMemFree(pszPath);
            }
            pItem->lpVtbl->Release(pItem);
        }
    }
    else {
        outPath[0] = '\0';
    }

    pDlg->lpVtbl->Release(pDlg);
    if (needUninit) CoUninitialize();
    return result;
}

// 扫描文件夹下所有字体文件（*.ttf *.otf *.ttc）
// 返回：找到的字体数量（最多 maxCount 个）
int ScanFontsInFolder(const char* folder, char outPaths[][512], int maxCount) {
    int count = 0;
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "%s\\*.*", folder);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (count >= maxCount) break;

        const char* name = fd.cFileName;
        const char* dot = strrchr(name, '.');
        if (!dot) continue;

        if (_stricmp(dot, ".ttf") == 0
            || _stricmp(dot, ".otf") == 0
            || _stricmp(dot, ".ttc") == 0)
        {
            snprintf(outPaths[count], 512, "%s\\%s", folder, name);
            ++count;
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return count;
}

// ---- 剪贴板 ----

// 复制文本到剪贴板
// 返回：1 = 成功，0 = 失败
int ClipboardCopy(const char* text) {
    if (!text) return 0;
    size_t len = strlen(text);
    if (len == 0) return 0;

    if (!OpenClipboard(NULL)) return 0;
    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (!hMem) {
        CloseClipboard();
        return 0;
    }

    char* p = (char*)GlobalLock(hMem);
    memcpy(p, text, len + 1);
    GlobalUnlock(hMem);

    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
    return 1;
}

// 从剪贴板读取文本（返回 malloc 的字符串，用完要 free）
// 返回：NULL = 失败，非 NULL = 成功
char* ClipboardPaste(void) {
    if (!OpenClipboard(NULL)) return NULL;

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) {
        CloseClipboard();
        return NULL;
    }

    char* p = (char*)GlobalLock(hData);
    if (!p) {
        CloseClipboard();
        return NULL;
    }

    size_t len = strlen(p);
    char* result = (char*)malloc(len + 1);
    if (result) {
        memcpy(result, p, len + 1);
    }

    GlobalUnlock(hData);
    CloseClipboard();
    return result;
}