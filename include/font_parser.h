#ifndef FONT_PARSER_H
#define FONT_PARSER_H

// 从 ttf/otf 文件读取字体名（Family Name）
// 成功返回 1，失败返回 0
// 失败时 outName 会被置为空字符串
// ttc 文件（字体集合）暂不支持，返回 0
int GetFontFamilyName(const char* path, char* outName, int outSize);

#endif