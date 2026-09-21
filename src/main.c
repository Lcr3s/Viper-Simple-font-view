#define _CRT_SECURE_NO_WARNINGS
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <string.h>
#include <raylib.h>
#include "theme.h"
#include "text_utils.h"
#include "ui.h"
#include "settings.h"
#include "font_parser.h"

int OpenFontFileDialog(char* outPath, int outPathSize);
int OpenFolderDialog(void* hwndOwner, char* outPath, int outPathSize);
void DisableWindowRoundedCorners(void* hwnd);
int ScanFontsInFolder(const char* folder, char outPaths[][512], int maxCount);
int ClipboardCopy(const char* text);
char* ClipboardPaste(void);

static int IsFontFile(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return 0;
    return _stricmp(dot, ".ttf") == 0
        || _stricmp(dot, ".otf") == 0
        || _stricmp(dot, ".ttc") == 0;
}

#define BTN_SIZE          44
#define FONT_SIZE_MIN     12
#define FONT_SIZE_MAX     64
#define FONT_SIZE_STEP    4
#define LOAD_FONT_SIZE    48
#define UI_FONT_SIZE      24
#define TEXT_LEFT_PAD     20
#define TEXT_TOP_PAD      20
#define LINE_SPACING      4
#define LIST_ITEM_HEIGHT  44
#define SEARCH_BOX_HEIGHT 40
#define MAX_CODEPOINTS    (128 + 3500)
#define SPLITTER_WIDTH    8
#define SCROLLBAR_WIDTH   12
#define SCROLLBAR_MIN_H   30
#define GAP               20

static int gCodepoints[MAX_CODEPOINTS];
static int gCodepointCount = 0;

static void BuildCodepoints(void) {
    if (gCodepointCount > 0) return;
    for (int cp = 32; cp < 127; ++cp)
        gCodepoints[gCodepointCount++] = cp;
    for (int cp = 0x4E00; cp < 0x4E00 + 3500; ++cp)
        gCodepoints[gCodepointCount++] = cp;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1000, 700, "Viper");
    SetTargetFPS(60);

    DisableWindowRoundedCorners(GetWindowHandle());

    BuildCodepoints();
    printf("Codepoints total: %d\n", gCodepointCount);

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L, "print('Input')");

    // ---- 读设置 ----
    Settings settings;
    SettingsLoad(&settings);
    float darkAnim = (float)settings.dark;

    // ---- 加载 UI 字体 ----
    Font uiFont;
    int  uiFontIsDefault = 1;
    if (settings.uiFont[0] != '\0' && IsFontFile(settings.uiFont)) {
        uiFont = LoadFontEx(settings.uiFont, UI_FONT_SIZE,
            gCodepoints, gCodepointCount);
    }
    else {
        uiFont = LoadFontEx("fonts/MonaspaceNeonFrozen-Regular.ttf",
            UI_FONT_SIZE, gCodepoints, gCodepointCount);
    }
    if (uiFont.texture.id != 0) {
        SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
        uiFontIsDefault = 0;
    }
    else {
        uiFont = GetFontDefault();
    }

    // ---- 加载图标 ----
    Texture2D iconFolder = LoadTexture("icons/folder-symlink.png");
    Texture2D iconEraser = LoadTexture("icons/eraser.png");
    Texture2D iconSettings = LoadTexture("icons/settings.png");
    Texture2D iconX = LoadTexture("icons/x.png");

    SetTextureFilter(iconFolder, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconEraser, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconSettings, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconX, TEXTURE_FILTER_BILINEAR);

    printf("Icons: folder=%u, eraser=%u, settings=%u, x=%u\n",
        iconFolder.id, iconEraser.id, iconSettings.id, iconX.id);

    char selectedFontPath[512] = "";

    Font previewFont = { 0 };
    int  previewFontLoaded = 0;
    int  loadFontSize = LOAD_FONT_SIZE;

    int previewFontSize = 32;

    char inputBuffer[512] = "";
    int  inputLength = 0;
    int  cursorPos = 0;
    int  inputFocused = 0;

    float backspaceTimer = 0.0f;

    char searchBuffer[128] = "";
    int  searchLength = 0;
    int  searchFocused = 0;

    static int visibleIdx[MAX_FONTS];
    int visibleCount = 0;

    float splitRatio = 0.65f;
    int draggingSplitter = 0;

    int draggingListThumb = 0;
    float listThumbDragStartY = 0;
    float listThumbDragStartScroll = 0;

    const int settingX = 20;
    const int settingY = 20;
    const int settingSize = 27;

    int settingsOpen = 0;
    int langDropdownOpen = 0;

    int btnX = settingX;
    int btnY = settingY + settingSize + 10;

    Rectangle btnOpenFolder = { (float)btnX, (float)btnY, BTN_SIZE, BTN_SIZE };
    Rectangle btnClear = { (float)btnX,
                           btnOpenFolder.y + btnOpenFolder.height,
                           BTN_SIZE, BTN_SIZE };

#define RELOAD_PREVIEW_FONT(path) do { \
        if (previewFontLoaded) { UnloadFont(previewFont); previewFontLoaded = 0; } \
        printf("Loading font: %s\n", (path)); \
        previewFont = LoadFontEx((path), loadFontSize, gCodepoints, gCodepointCount); \
        printf("  texture.id=%u, glyphCount=%d, baseSize=%d\n", \
               previewFont.texture.id, previewFont.glyphCount, previewFont.baseSize); \
        if (previewFont.texture.id != 0) { \
            SetTextureFilter(previewFont.texture, TEXTURE_FILTER_BILINEAR); \
            previewFontLoaded = 1; \
        } \
    } while (0)

    while (!WindowShouldClose()) {
        bool mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        // ---- 主题动画 ----
        float target = (float)settings.dark;
        if (darkAnim < target) {
            darkAnim += GetFrameTime() * 4.0f;
            if (darkAnim > target) darkAnim = target;
        }
        else if (darkAnim > target) {
            darkAnim -= GetFrameTime() * 4.0f;
            if (darkAnim < target) darkAnim = target;
        }
        Theme T = BlendTheme(darkAnim, &themeLight, &themeDark);

        Vector2 mouse = GetMousePosition();
        bool hoverOpenFolder = CheckCollisionPointRec(mouse, btnOpenFolder);
        bool hoverClear = CheckCollisionPointRec(mouse, btnClear);

        Rectangle btnSettings = {
            (float)btnX,
            (float)GetScreenHeight() - BTN_SIZE - 20,
            BTN_SIZE, BTN_SIZE
        };
        bool hoverSettings = CheckCollisionPointRec(mouse, btnSettings);

        // ---- 布局 ----
        float previewTop = btnOpenFolder.y;
        float previewBottom = (float)GetScreenHeight() - 20;
        float totalH = previewBottom - previewTop;

        float contentX = btnOpenFolder.x + btnOpenFolder.width + GAP;
        float contentW = (float)GetScreenWidth() - contentX - GAP;
        float usableW = contentW - GAP;

        float previewW = usableW * splitRatio;
        float listW = usableW - previewW;

        if (listW < 200) { listW = 200; previewW = usableW - listW; }
        if (previewW < 200) { previewW = 200; listW = usableW - previewW; }

        Rectangle previewBox = { contentX, previewTop, previewW, totalH };

        float listX = contentX + previewW + GAP;
        float listY = previewTop;

        // 搜索框（只有输入区 + × 按钮）
        Rectangle searchClearBtn = {
            listX + listW - SEARCH_BOX_HEIGHT,
            listY,
            SEARCH_BOX_HEIGHT,
            SEARCH_BOX_HEIGHT
        };
        Rectangle searchBox = {
            listX,
            listY,
            listW - SEARCH_BOX_HEIGHT,
            SEARCH_BOX_HEIGHT
        };

        float listAreaY = listY + SEARCH_BOX_HEIGHT;
        float listAreaH = totalH - SEARCH_BOX_HEIGHT;

        Rectangle listBox = { listX, listAreaY, listW, listAreaH };

        Rectangle scrollTrack = {
            listX + listW - SCROLLBAR_WIDTH - 2,
            listAreaY + 2,
            SCROLLBAR_WIDTH,
            listAreaH - 4
        };

        Rectangle splitter = {
            contentX + previewW + (GAP - SPLITTER_WIDTH) / 2.0f,
            previewTop,
            SPLITTER_WIDTH,
            totalH
        };

        bool hoverPreview = CheckCollisionPointRec(mouse, previewBox);
        bool hoverList = CheckCollisionPointRec(mouse, listBox);
        bool hoverSearch = CheckCollisionPointRec(mouse, searchBox);
        bool hoverSearchClear = CheckCollisionPointRec(mouse, searchClearBtn);
        bool hoverSplitter = CheckCollisionPointRec(mouse, splitter);

        // ---- 设置面板布局 ----
#define PANEL_W 320
#define PANEL_H 260
        Rectangle settingsPanel = { 0, 0, 0, 0 };
        Rectangle rowLang, rowDark, rowFont;
        Rectangle switchDark, langBox;
        bool hoverPanel = false;

        float panelX = btnSettings.x;
        float panelY = btnSettings.y - PANEL_H - 8;
        if (panelY < 10) panelY = 10;
        settingsPanel = (Rectangle){ panelX, panelY, PANEL_W, PANEL_H };

        rowLang = (Rectangle){ settingsPanel.x + 16, settingsPanel.y + 50,  PANEL_W - 32, 32 };
        rowDark = (Rectangle){ settingsPanel.x + 16, settingsPanel.y + 110, PANEL_W - 32, 32 };
        rowFont = (Rectangle){ settingsPanel.x + 16, settingsPanel.y + 170, PANEL_W - 32, 32 };

        float switchW = 50, switchH = 26;
        switchDark = (Rectangle){ rowDark.x + rowDark.width - switchW - 8,
                                  rowDark.y + (rowDark.height - switchH) / 2,
                                  switchW, switchH };

        langBox = (Rectangle){ rowLang.x + rowLang.width - 120,
                               rowLang.y, 120, rowLang.height };

        hoverPanel = settingsOpen && CheckCollisionPointRec(mouse, settingsPanel);

        // ---- 是否需要滚动条 ----
        float listContentH = visibleCount * LIST_ITEM_HEIGHT;
        int needScrollbar = (listContentH > listAreaH);

        Rectangle listThumb = { 0, 0, 0, 0 };
        if (needScrollbar) {
            float thumbH = scrollTrack.height * (listAreaH / listContentH);
            if (thumbH < SCROLLBAR_MIN_H) thumbH = SCROLLBAR_MIN_H;
            if (thumbH > scrollTrack.height) thumbH = scrollTrack.height;

            float maxScroll = listContentH - listAreaH;
            float scrollRatio = (maxScroll > 0) ? (fontListScroll / maxScroll) : 0;
            float thumbTravel = scrollTrack.height - thumbH;
            float thumbY = scrollTrack.y + scrollRatio * thumbTravel;

            listThumb = (Rectangle){
                scrollTrack.x, thumbY,
                SCROLLBAR_WIDTH, thumbH
            };
        }
        bool hoverListThumb = needScrollbar &&
            CheckCollisionPointRec(mouse, listThumb);

        // ---- 拖动分隔条 ----
        if (hoverSplitter && mousePressed) {
            draggingSplitter = 1;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            draggingSplitter = 0;
        }
        if (draggingSplitter && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float newPreviewW = mouse.x - contentX;
            splitRatio = newPreviewW / usableW;
            if (splitRatio < 0.2f) splitRatio = 0.2f;
            if (splitRatio > 0.8f) splitRatio = 0.8f;
        }

        // ---- 拖动列表滚动条滑块 ----
        if (hoverListThumb && mousePressed) {
            draggingListThumb = 1;
            listThumbDragStartY = mouse.y;
            listThumbDragStartScroll = fontListScroll;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            draggingListThumb = 0;
        }
        if (draggingListThumb && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float delta = mouse.y - listThumbDragStartY;
            float maxScroll = listContentH - listAreaH;
            float thumbTravel = scrollTrack.height - listThumb.height;
            if (thumbTravel > 0) {
                float newScroll = listThumbDragStartScroll
                    + delta * maxScroll / thumbTravel;
                if (newScroll < 0) newScroll = 0;
                if (newScroll > maxScroll) newScroll = maxScroll;
                fontListScroll = newScroll;
            }
        }

        if (draggingSplitter || hoverSplitter) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        }
        else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }

        // ---- 构建可见列表 ----
        visibleCount = 0;
        for (int i = 0; i < fontListCount; ++i) {
            const char* target = BaseName(fontList[i]);
            if (searchLength == 0 || ContainsNoCase(target, searchBuffer)) {
                visibleIdx[visibleCount++] = i;
            }
        }

        // ================= 设置面板输入处理 =================
        if (settingsOpen) {
            if (mousePressed && !hoverPanel && !hoverSettings) {
                settingsOpen = 0;
                langDropdownOpen = 0;
            }
            else {
                // 语言下拉
                if (mousePressed && CheckCollisionPointRec(mouse, langBox)) {
                    langDropdownOpen = !langDropdownOpen;
                }
                if (langDropdownOpen) {
                    Rectangle opt0 = { langBox.x, langBox.y + langBox.height,
                                       langBox.width, 28 };
                    Rectangle opt1 = { langBox.x, langBox.y + langBox.height + 28,
                                       langBox.width, 28 };
                    if (mousePressed && CheckCollisionPointRec(mouse, opt0)) {
                        settings.language = 0;
                        langDropdownOpen = 0;
                        SettingsSave(&settings);
                    }
                    if (mousePressed && CheckCollisionPointRec(mouse, opt1)) {
                        settings.language = 1;
                        langDropdownOpen = 0;
                        SettingsSave(&settings);
                    }
                }

                // 夜间开关
                if (mousePressed && CheckCollisionPointRec(mouse, switchDark)) {
                    settings.dark = !settings.dark;
                    SettingsSave(&settings);
                }

                // 默认 UI 字体
                if (mousePressed && CheckCollisionPointRec(mouse, rowFont)) {
                    char path[512] = "";
                    if (OpenFontFileDialog(path, sizeof(path))) {
                        if (IsFontFile(path)) {
                            Font newFont = LoadFontEx(path, UI_FONT_SIZE,
                                gCodepoints, gCodepointCount);
                            if (newFont.texture.id != 0) {
                                SetTextureFilter(newFont.texture, TEXTURE_FILTER_BILINEAR);
                                if (!uiFontIsDefault && uiFont.texture.id != 0)
                                    UnloadFont(uiFont);
                                uiFont = newFont;
                                uiFontIsDefault = 0;
                                strncpy(settings.uiFont, path, sizeof(settings.uiFont) - 1);
                                settings.uiFont[sizeof(settings.uiFont) - 1] = '\0';
                                SettingsSave(&settings);
                            }
                        }
                    }
                }
            }
        }

        // ---- 点击设置按钮：开关面板 ----
        if (hoverSettings && mousePressed) {
            settingsOpen = !settingsOpen;
            if (!settingsOpen) langDropdownOpen = 0;
        }

        // ---- 选文件夹 ----
        if (hoverOpenFolder && mousePressed) {
            char folder[512] = "";
            if (OpenFolderDialog(GetWindowHandle(), folder, sizeof(folder))) {
                printf("Selected folder: %s\n", folder);
                fontListCount = ScanFontsInFolder(folder, fontList, MAX_FONTS);
                printf("Found %d fonts\n", fontListCount);
                // 逐个读字体名
                for (int i = 0; i < fontListCount; ++i) {
                    fontNames[i][0] = '\0';
                    if (GetFontFamilyName(fontList[i], fontNames[i], 128)) {
                        // 成功
                    }
                    else {
                        // 失败（ttc 或不支持的格式），用文件名
                        strncpy(fontNames[i], BaseName(fontList[i]), 127);
                        fontNames[i][127] = '\0';
                    }
                }
                fontListSelected = -1;
                fontListScroll = 0.0f;
                searchBuffer[0] = '\0';
                searchLength = 0;
            }
        }

        // ---- 点击列表项 ----
        if (hoverList && mousePressed) {
            int localY = (int)(mouse.y - listBox.y + fontListScroll);
            int v = localY / LIST_ITEM_HEIGHT;
            if (v >= 0 && v < visibleCount) {
                int idx = visibleIdx[v];
                fontListSelected = idx;
                strcpy(selectedFontPath, fontList[idx]);
                printf("List selected: %s\n", selectedFontPath);
                RELOAD_PREVIEW_FONT(selectedFontPath);
                inputFocused = 1;
            }
        }

        // ---- 点击搜索框 ----
        if (hoverSearch && mousePressed) {
            searchFocused = 1;
            inputFocused = 0;
        }

        // ---- 点击搜索框清除按钮 ----
        if (hoverSearchClear && mousePressed) {
            searchBuffer[0] = '\0';
            searchLength = 0;
            searchFocused = 1;
        }

        // ---- 列表滚动（滚轮）----
        if (hoverList) {
            float wheel = GetMouseWheelMove();
            if (needScrollbar) {
                fontListScroll -= wheel * LIST_ITEM_HEIGHT;
                float maxScroll = listContentH - listAreaH;
                if (fontListScroll < 0) fontListScroll = 0;
                if (fontListScroll > maxScroll) fontListScroll = maxScroll;
            }
        }

        // ---- 点击预览框：聚焦 + 定位光标 ----
        if (hoverPreview && mousePressed) {
            inputFocused = 1;
            searchFocused = 0;
            Font useFont = previewFontLoaded ? previewFont : GetFontDefault();
            float textX = previewBox.x + TEXT_LEFT_PAD;
            float textY = previewBox.y + TEXT_TOP_PAD;
            float relY = mouse.y - textY;

            int lineStart[64], lineEnd[64];
            int lines = SplitLines(inputBuffer, inputLength,
                lineStart, lineEnd, 64);
            float lineH = (float)previewFontSize + LINE_SPACING;

            int hitLine = (int)(relY / lineH);
            if (hitLine < 0) hitLine = 0;
            if (hitLine > lines - 1) hitLine = lines - 1;

            int hitCol = 0;
            float relX = mouse.x - textX;
            if (relX < 0) relX = 0;

            for (int c = lineStart[hitLine]; c <= lineEnd[hitLine]; ++c) {
                int plen = c - lineStart[hitLine];
                if (plen > 511) plen = 511;
                char prefix[512];
                memcpy(prefix, inputBuffer + lineStart[hitLine], plen);
                prefix[plen] = '\0';
                float w = MeasureTextEx(useFont, prefix,
                    (float)previewFontSize, 2).x;
                if (w > relX) {
                    hitCol = plen;
                    break;
                }
                hitCol = plen;
            }

            cursorPos = lineStart[hitLine] + hitCol;
            if (cursorPos > inputLength) cursorPos = inputLength;
        }

        if (mousePressed
            && !hoverPreview && !hoverOpenFolder
            && !hoverClear && !hoverList && !hoverSettings
            && !hoverSearch && !hoverSplitter
            && !hoverSearchClear && !hoverListThumb)
        {
            inputFocused = 0;
            searchFocused = 0;
        }

        // ---- 清空按钮 ----
        if (hoverClear && mousePressed) {
            if (previewFontLoaded) {
                UnloadFont(previewFont);
                previewFontLoaded = 0;
                previewFont.texture.id = 0;
            }
            selectedFontPath[0] = '\0';
            inputBuffer[0] = '\0';
            inputLength = 0;
            cursorPos = 0;
            inputFocused = 0;
            //#fontListCount = 0;
            //#fontListSelected = -1;
            //#fontListScroll = 0.0f;
            searchBuffer[0] = '\0';
            searchLength = 0;
            searchFocused = 0;
            printf("Cleared\n");
        }

        // ---- 字号调整 ----
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
                previewFontSize += FONT_SIZE_STEP;
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
                previewFontSize -= FONT_SIZE_STEP;
        }
        if (hoverPreview) {
            float wheel = GetMouseWheelMove();
            if (wheel > 0) previewFontSize += FONT_SIZE_STEP;
            if (wheel < 0) previewFontSize -= FONT_SIZE_STEP;
        }
        if (previewFontSize < FONT_SIZE_MIN) previewFontSize = FONT_SIZE_MIN;
        if (previewFontSize > FONT_SIZE_MAX) previewFontSize = FONT_SIZE_MAX;

        // ---- 搜索框输入 ----
        if (searchFocused) {
            int ch;
            while ((ch = GetCharPressed()) > 0) {
                if (ch < 32) continue;
                char utf8[5] = { 0 };
                int n = EncodeUTF8(ch, utf8);
                if (searchLength + n > 127) break;
                for (int i = 0; i < n; ++i)
                    searchBuffer[searchLength + i] = utf8[i];
                searchLength += n;
                searchBuffer[searchLength] = '\0';
            }
            if (IsKeyPressed(KEY_BACKSPACE) && searchLength > 0) {
                int n = PrevUTF8CharLen(searchBuffer, searchLength);
                searchLength -= n;
                searchBuffer[searchLength] = '\0';
            }
        }

        // ---- 预览框输入 ----
        if (inputFocused) {
            if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                if (IsKeyPressed(KEY_C)) {
                    if (inputLength > 0) ClipboardCopy(inputBuffer);
                }
                if (IsKeyPressed(KEY_V)) {
                    char* clip = ClipboardPaste();
                    if (clip) {
                        int clipLen = (int)strlen(clip);
                        for (int i = 0; i < clipLen; ++i) {
                            if (clip[i] < 32 && clip[i] != '\n') continue;
                            if (inputLength + 1 > 510) break;
                            for (int j = inputLength; j > cursorPos; --j)
                                inputBuffer[j] = inputBuffer[j - 1];
                            inputBuffer[cursorPos] = clip[i];
                            ++inputLength;
                            ++cursorPos;
                            inputBuffer[inputLength] = '\0';
                        }
                        free(clip);
                    }
                }
            }
            int ch;
            while ((ch = GetCharPressed()) > 0) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    if (IsKeyPressed(KEY_C)) {
                        if (inputLength > 0) ClipboardCopy(inputBuffer);
                    }
                    if (IsKeyPressed(KEY_V)) {
                        char* clip = ClipboardPaste();
                        if (clip) {
                            int clipLen = (int)strlen(clip);
                            for (int i = 0; i < clipLen; ++i) {
                                if (clip[i] < 32 && clip[i] != '\n') continue;
                                if (inputLength + 1 > 510) break;
                                for (int j = inputLength; j > cursorPos; --j)
                                    inputBuffer[j] = inputBuffer[j - 1];
                                inputBuffer[cursorPos] = clip[i];
                                ++inputLength;
                                ++cursorPos;
                                inputBuffer[inputLength] = '\0';
                            }
                            free(clip);
                        }
                    }
                }
                if (ch < 32) continue;
                char utf8[5] = { 0 };
                int n = EncodeUTF8(ch, utf8);
                if (inputLength + n > 510) break;
                for (int i = inputLength; i > cursorPos; --i)
                    inputBuffer[i + n - 1] = inputBuffer[i - 1];
                for (int i = 0; i < n; ++i)
                    inputBuffer[cursorPos + i] = utf8[i];
                inputLength += n;
                cursorPos += n;
                inputBuffer[inputLength] = '\0';
            }

            if (IsKeyPressed(KEY_ENTER)) {
                if (inputLength < 511) {
                    for (int i = inputLength; i > cursorPos; --i)
                        inputBuffer[i] = inputBuffer[i - 1];
                    inputBuffer[cursorPos] = '\n';
                    ++inputLength;
                    ++cursorPos;
                    inputBuffer[inputLength] = '\0';
                }
            }

            {
                int doDelete = 0;
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    doDelete = 1;
                    backspaceTimer = 0.0f;
                }
                else if (IsKeyDown(KEY_BACKSPACE)) {
                    backspaceTimer += GetFrameTime();
                    if (backspaceTimer > 0.3f) {
                        backspaceTimer -= 0.05f;
                        doDelete = 1;
                    }
                }
                else {
                    backspaceTimer = 0.0f;
                }
                if (doDelete && cursorPos > 0) {
                    int n = PrevUTF8CharLen(inputBuffer, cursorPos);
                    for (int i = cursorPos - n; i < inputLength - n; ++i)
                        inputBuffer[i] = inputBuffer[i + n];
                    inputLength -= n;
                    cursorPos -= n;
                    inputBuffer[inputLength] = '\0';
                }
            }

            if (IsKeyPressed(KEY_LEFT) && cursorPos > 0) {
                cursorPos -= PrevUTF8CharLen(inputBuffer, cursorPos);
            }
            if (IsKeyPressed(KEY_RIGHT) && cursorPos < inputLength) {
                cursorPos += PrevUTF8CharLen(inputBuffer, cursorPos + 1);
            }

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
                int lineStart[64], lineEnd[64];
                int lines = SplitLines(inputBuffer, inputLength,
                    lineStart, lineEnd, 64);
                int curLine, curCol;
                GetLineCol(inputBuffer, cursorPos, lines,
                    lineStart, lineEnd, &curLine, &curCol);
                int targetLine = curLine + (IsKeyPressed(KEY_DOWN) ? 1 : -1);
                if (targetLine < 0) targetLine = 0;
                if (targetLine > lines - 1) targetLine = lines - 1;
                int lineLen = lineEnd[targetLine] - lineStart[targetLine];
                int targetCol = curCol < lineLen ? curCol : lineLen;
                cursorPos = lineStart[targetLine] + targetCol;
            }
        }

        // ================= 绘制 =================
        BeginDrawing();
        ClearBackground(T.bg);

        DrawTextEx(uiFont, "Viper: free simple font viewer",
            (Vector2) {
            (float)settingX, (float)settingY
        },
            (float)settingSize, 2, T.text);

        Color colorOpen = hoverOpenFolder ? T.btnHover : T.bg;
        DrawRectangleRec(btnOpenFolder, colorOpen);

        Color colorClear = hoverClear ? T.btnHover : T.bg;
        DrawRectangleRec(btnClear, colorClear);

        DrawRectangleLinesEx(
            (Rectangle) {
            btnOpenFolder.x,
                btnOpenFolder.y,
                btnOpenFolder.width,
                btnOpenFolder.height + btnClear.height
        },
            2, T.border);

        DrawLine(
            (int)btnOpenFolder.x,
            (int)(btnOpenFolder.y + btnOpenFolder.height),
            (int)(btnOpenFolder.x + btnOpenFolder.width),
            (int)(btnOpenFolder.y + btnOpenFolder.height),
            T.border);

        DrawIcon(iconFolder, btnOpenFolder);
        DrawIcon(iconEraser, btnClear);

        Color colorSettings = hoverSettings ? T.btnHover : T.bg;
        DrawRectangleRec(btnSettings, colorSettings);
        DrawRectangleLinesEx(btnSettings, 2, T.border);
        DrawIcon(iconSettings, btnSettings);

        Color splitterColor = (draggingSplitter || hoverSplitter)
            ? T.border
            : T.inputBg;
        DrawRectangleRec(splitter, splitterColor);

        Color boxBorder = inputFocused ? T.borderActive : T.border;
        DrawRectangleLinesEx(previewBox, 2, boxBorder);

        Font useFont = previewFontLoaded ? previewFont : GetFontDefault();
        float textX = previewBox.x + TEXT_LEFT_PAD;
        float textY = previewBox.y + TEXT_TOP_PAD;
        float lineH = (float)previewFontSize + LINE_SPACING;

        BeginScissorMode((int)previewBox.x, (int)previewBox.y,
            (int)previewBox.width, (int)previewBox.height);

        int lineStart[64], lineEnd[64];
        int lines = SplitLines(inputBuffer, inputLength,
            lineStart, lineEnd, 64);

        if (inputLength == 0) {
            const char* hint = previewFontLoaded
                ? "Type here..."
                : "Select a folder first (top-left button)";
            DrawTextEx(uiFont, hint,
                (Vector2) {
                textX, textY
            },
                (float)previewFontSize, 2, T.textDim);
        }
        else {
            float maxWidth = previewBox.width - TEXT_LEFT_PAD * 2;
            float curY = textY;

            for (int i = 0; i < lines; ++i) {
                int ls = lineStart[i];
                int le = lineEnd[i];

                // 空行也要占一行
                if (ls == le) {
                    curY += lineH;
                    continue;
                }

                int segStart[64], segEnd[64];
                int segs = WrapLine(inputBuffer, ls, le,
                    useFont, (float)previewFontSize, maxWidth,
                    segStart, segEnd, 64);

                for (int s = 0; s < segs; ++s) {
                    int len = segEnd[s] - segStart[s];
                    if (len <= 0) continue;
                    char lineBuf[512];
                    if (len > 511) len = 511;
                    memcpy(lineBuf, inputBuffer + segStart[s], len);
                    lineBuf[len] = '\0';

                    DrawTextEx(useFont, lineBuf,
                        (Vector2) {
                        textX, curY
                    },
                        (float)previewFontSize, 2, T.text);

                    curY += lineH;
                }
            }
        }

        if (inputFocused) {
            float maxWidth = previewBox.width - TEXT_LEFT_PAD * 2;

            // 1. 光标在原文的第几行、第几列
            int curLine, curCol;
            GetLineCol(inputBuffer, cursorPos, lines,
                lineStart, lineEnd, &curLine, &curCol);

            // 2. 光标所在那行的起止
            int ls = lineStart[curLine];
            int le = lineEnd[curLine];

            // 3. 对那行做 WrapLine
            int segStart[64], segEnd[64];
            int segs = WrapLine(inputBuffer, ls, le,
                useFont, (float)previewFontSize, maxWidth,
                segStart, segEnd, 64);

            // 4. 光标绝对位置
            int targetPos = ls + curCol;

            // 5. 找光标在第几个视觉段
            int targetSeg = 0;
            if (segs > 0) {
                for (int s = 0; s < segs; ++s) {
                    if (targetPos <= segEnd[s]) {
                        targetSeg = s;
                        break;
                    }
                    targetSeg = s;
                }
            }

            // 6. 光标在段内的列
            int posInSeg = targetPos - segStart[targetSeg];
            if (posInSeg < 0) posInSeg = 0;

            // 7. 算光标 Y：把光标之前的所有视觉行加起来
            float curY = textY;
            for (int i = 0; i < curLine; ++i) {
                int ls2 = lineStart[i];
                int le2 = lineEnd[i];
                if (ls2 == le2) {
                    curY += lineH;
                    continue;
                }
                int ss2[64], se2[64];
                int segs2 = WrapLine(inputBuffer, ls2, le2,
                    useFont, (float)previewFontSize, maxWidth,
                    ss2, se2, 64);
                curY += segs2 * lineH;
            }
            curY += targetSeg * lineH;

            // 8. 光标 X：测前缀宽度
            char prefix[512];
            int plen = posInSeg;
            if (plen > 511) plen = 511;
            memcpy(prefix, inputBuffer + segStart[targetSeg], plen);
            prefix[plen] = '\0';

            float prefixW = MeasureTextEx(useFont, prefix,
                (float)previewFontSize, 2).x;

            float caretX = textX + prefixW;
            float caretY = curY + 4;
            float caretH = (float)previewFontSize - 10;

            if (((int)(GetTime() * 2)) % 2 == 0) {
                DrawRectangle((int)caretX, (int)caretY, 2, (int)caretH, T.borderActive);
            }
        }

        EndScissorMode();

        // ---- 左下角小框：用字体自身渲染字体名 ----
        if (previewFontLoaded && fontListSelected >= 0) {
            const char* name = fontNames[fontListSelected];
            if (name[0] == '\0') name = BaseName(fontList[fontListSelected]);

            float nameFontSize = 32;
            Vector2 nameSize = MeasureTextEx(previewFont, name, nameFontSize, 2);

            float boxW = nameSize.x + 24;
            float boxH = 56;
            if (boxW < 60) boxW = 60;

            Rectangle nameBox = {
                previewBox.x + 10,
                previewBox.y + previewBox.height - boxH - 10,
                boxW, boxH
            };

            DrawRectangleRec(nameBox, T.panelBg);
            DrawRectangleLinesEx(nameBox, 2, T.border);

            float tx = nameBox.x + (nameBox.width - nameSize.x) / 2;
            float ty = nameBox.y + (nameBox.height - nameSize.y) / 2;
            DrawTextEx(previewFont, name, (Vector2) { tx, ty },
                nameFontSize, 2, T.text);
        }

        // ---- 搜索框 ----
        DrawRectangleRec(searchBox, searchFocused ? T.bg : T.inputBg);
        DrawRectangleLinesEx(searchBox, 2, searchFocused ? T.borderActive : T.border);

        if (searchLength == 0) {
            DrawTextEx(uiFont, "Search...",
                (Vector2) {
                searchBox.x + 10, searchBox.y + 8
            },
                20, 2, T.textDim);
        }
        else {
            DrawTextEx(uiFont, searchBuffer,
                (Vector2) {
                searchBox.x + 10, searchBox.y + 8
            },
                20, 2, T.text);
        }

        if (searchFocused) {
            float sw = MeasureTextEx(uiFont, searchBuffer, 20, 2).x;
            float cX = searchBox.x + 10 + sw + 2;
            float cY = searchBox.y + 8;
            if (((int)(GetTime() * 2)) % 2 == 0) {
                DrawRectangle((int)cX, (int)cY, 2, 22, T.text);
            }
        }

        // × 按钮
        DrawRectangleRec(searchClearBtn,
            hoverSearchClear ? T.btnHover : T.bg);
        DrawRectangleLinesEx(searchClearBtn, 2, T.border);
        DrawIcon(iconX, searchClearBtn);

        // ---- 字体列表 ----
        DrawRectangleLinesEx(listBox, 2, T.border);

        BeginScissorMode((int)listBox.x, (int)listBox.y,
            (int)listBox.width, (int)listBox.height);

        for (int v = 0; v < visibleCount; ++v) {
            int i = visibleIdx[v];
            float itemY = listBox.y + v * LIST_ITEM_HEIGHT - fontListScroll;
            if (itemY + LIST_ITEM_HEIGHT < listBox.y) continue;
            if (itemY > listBox.y + listBox.height) break;

            Rectangle itemRect = { listBox.x, itemY, listBox.width, LIST_ITEM_HEIGHT };

            if (i == fontListSelected) {
                DrawRectangleRec(itemRect, T.listSel);
            }

            const char* display = BaseName(fontList[i]);
            DrawTextEx(uiFont, display,
                (Vector2) {
                itemRect.x + 8,
                    itemRect.y + (LIST_ITEM_HEIGHT - 28) / 2.0f
            },
                28, 2, T.text);
        }

        EndScissorMode();

        if (needScrollbar) {
            DrawRectangleRec(scrollTrack, T.scrollTrack);
            Color thumbColor = hoverListThumb
                ? T.scrollThumbHover
                : T.scrollThumb;
            DrawRectangleRec(listThumb, thumbColor);
        }

        // ---- 设置面板 ----
        if (settingsOpen) {
            DrawRectangleRec(settingsPanel, T.panelBg);
            DrawRectangleLinesEx(settingsPanel, 2, T.border);

            DrawTextEx(uiFont, "Settings",
                (Vector2) {
                settingsPanel.x + 16, settingsPanel.y + 12
            },
                20, 2, T.text);

            DrawTextEx(uiFont, "Language",
                (Vector2) {
                rowLang.x, rowLang.y + 8
            },
                18, 2, T.text);
            DrawRectangleRec(langBox, T.inputBg);
            DrawRectangleLinesEx(langBox, 2, T.border);
            const char* langNames[2] = { "中文", "English" };
            DrawTextEx(uiFont, langNames[settings.language],
                (Vector2) {
                langBox.x + 8, langBox.y + 8
            },
                18, 2, T.text);

            if (langDropdownOpen) {
                Rectangle opt0 = { langBox.x, langBox.y + langBox.height, langBox.width, 28 };
                Rectangle opt1 = { langBox.x, langBox.y + langBox.height + 28, langBox.width, 28 };
                DrawRectangleRec(opt0, T.panelBg);
                DrawRectangleRec(opt1, T.panelBg);
                DrawRectangleLinesEx(opt0, 1, T.border);
                DrawRectangleLinesEx(opt1, 1, T.border);
                DrawTextEx(uiFont, "中文", (Vector2) { opt0.x + 8, opt0.y + 6 }, 18, 2, T.text);
                DrawTextEx(uiFont, "English", (Vector2) { opt1.x + 8, opt1.y + 6 }, 18, 2, T.text);
            }

            DrawTextEx(uiFont, "Dark Mode",
                (Vector2) {
                rowDark.x, rowDark.y + 8
            },
                18, 2, T.text);
            Color switchBg = settings.dark ? T.switchBgOn : T.switchBgOff;
            DrawRectangleRec(switchDark, switchBg);
            DrawRectangleLinesEx(switchDark, 2, T.border);

            float knobW = switchH - 8;
            float knobH = switchH - 8;
            float knobX = settings.dark
                ? switchDark.x + switchDark.width - knobW - 4
                : switchDark.x + 4;
            float knobY = switchDark.y + 4;
            DrawRectangle((int)knobX, (int)knobY, (int)knobW, (int)knobH, T.switchKnob);

            DrawTextEx(uiFont, "UI Font",
                (Vector2) {
                rowFont.x, rowFont.y + 8
            },
                18, 2, T.text);
            const char* fontLabel = settings.uiFont[0] ? "Custom" : "Default";
            DrawTextEx(uiFont, fontLabel,
                (Vector2) {
                rowFont.x + rowFont.width - 80, rowFont.y + 8
            },
                18, 2, T.textDim);
        }

        EndDrawing();
    }

    // ---- 释放 ----
    UnloadTexture(iconFolder);
    UnloadTexture(iconEraser);
    UnloadTexture(iconSettings);
    UnloadTexture(iconX);

    if (!uiFontIsDefault && uiFont.texture.id != 0) {
        UnloadFont(uiFont);
    }

    if (previewFontLoaded) UnloadFont(previewFont);
    lua_close(L);
    CloseWindow();
    return 0;
}