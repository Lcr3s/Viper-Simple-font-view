#pragma once
#include <raylib.h>
#include <stddef.h>

int SplitLines(const char* buf, int len,
    int* lineStart, int* lineEnd, int maxLines);

void GetLineCol(const char* buf, int cursorPos, int lines,
    int* lineStart, int* lineEnd,
    int* outLine, int* outCol);

const char* BaseName(const char* path);

int EncodeUTF8(int codepoint, char* out);

int PrevUTF8CharLen(const char* buf, int pos);

int ContainsNoCase(const char* haystack, const char* needle);

// 把 buffer[start..end) 按宽度自动折行
// 每段的起止下标写入 segStart[], segEnd[]
// 返回段数
int WrapLine(const char* buf, int start, int end,
    Font font, float fontSize, float maxWidth,
    int* segStart, int* segEnd, int maxSegs);