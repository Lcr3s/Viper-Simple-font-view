#include "text_utils.h"
#include <string.h>
#include <raylib.h>

int SplitLines(const char* buf, int len,
    int* lineStart, int* lineEnd, int maxLines) {
    int lines = 0;
    int start = 0;
    for (int i = 0; i <= len; ++i) {
        if (i == len || buf[i] == '\n') {
            if (lines < maxLines) {
                lineStart[lines] = start;
                lineEnd[lines] = i;
            }
            ++lines;
            start = i + 1;
        }
    }
    return lines;
}

void GetLineCol(const char* buf, int cursorPos, int lines,
    int* lineStart, int* lineEnd,
    int* outLine, int* outCol) {
    for (int i = 0; i < lines; ++i) {
        if (cursorPos >= lineStart[i] && cursorPos <= lineEnd[i]) {
            *outLine = i;
            *outCol = cursorPos - lineStart[i];
            return;
        }
    }
    *outLine = lines - 1;
    *outCol = lineEnd[lines - 1] - lineStart[lines - 1];
}

const char* BaseName(const char* path) {
    const char* s = strrchr(path, '\\');
    if (!s) s = strrchr(path, '/');
    return s ? s + 1 : path;
}

int EncodeUTF8(int codepoint, char* out) {
    if (codepoint < 0x80) {
        out[0] = (char)codepoint;
        return 1;
    }
    else if (codepoint < 0x800) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    }
    else if (codepoint < 0x10000) {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }
    else {
        out[0] = (char)(0xF0 | (codepoint >> 18));
        out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }
}

int PrevUTF8CharLen(const char* buf, int pos) {
    if (pos <= 0) return 0;
    int i = pos - 1;
    while (i > 0 && (buf[i] & 0xC0) == 0x80) --i;
    return pos - i;
}

int ContainsNoCase(const char* haystack, const char* needle) {
    if (!needle || needle[0] == '\0') return 1;
    size_t nl = strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        if (_strnicmp(p, needle, nl) == 0) return 1;
    }
    return 0;
}

int WrapLine(const char* buf, int start, int end,
    Font font, float fontSize, float maxWidth,
    int* segStart, int* segEnd, int maxSegs) {
    int segs = 0;
    int segBegin = start;
    int lastSpace = -1;

    for (int i = start; i < end; ++i) {
        if (buf[i] == ' ') lastSpace = i;

        // 测量 segBegin 到 i+1 的宽度
        int len = i + 1 - segBegin;
        if (len > 511) len = 511;

        char tmp[512];
        memcpy(tmp, buf + segBegin, len);
        tmp[len] = '\0';

        Vector2 sz = MeasureTextEx(font, tmp, fontSize, 2);
        if (sz.x > maxWidth && segBegin < i) {
            // 超宽，折行
            int breakAt;
            if (lastSpace > segBegin) {
                breakAt = lastSpace;   // 在空格处折
            }
            else {
                breakAt = i;           // 无处可折，强制在当前位置折
            }

            if (segs < maxSegs) {
                segStart[segs] = segBegin;
                segEnd[segs] = breakAt;
                ++segs;
            }
            segBegin = breakAt;
            if (buf[segBegin] == ' ') segBegin++;
            lastSpace = -1;
        }
    }

    // 最后一段
    if (segBegin < end && segs < maxSegs) {
        segStart[segs] = segBegin;
        segEnd[segs] = end;
        ++segs;
    }

    return segs;
}