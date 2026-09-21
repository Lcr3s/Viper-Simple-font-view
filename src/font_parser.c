#define _CRT_SECURE_NO_WARNINGS
#include "font_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 读取大端 16 位
static uint16_t ReadBE16(const unsigned char* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

// 读取大端 32 位
static uint32_t ReadBE32(const unsigned char* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

// UTF-16BE 转 UTF-8
static void UTF16BEToUTF8(const unsigned char* utf16, int byteCount,
    char* out, int outSize) {
    int outIdx = 0;
    int n = byteCount / 2;
    for (int i = 0; i < n; ++i) {
        uint16_t w = (uint16_t)((utf16[i * 2] << 8) | utf16[i * 2 + 1]);

        // 代理对简单跳过
        if (w >= 0xD800 && w <= 0xDFFF) continue;

        if (w < 0x80) {
            if (outIdx + 1 >= outSize) break;
            out[outIdx++] = (char)w;
        }
        else if (w < 0x800) {
            if (outIdx + 2 >= outSize) break;
            out[outIdx++] = (char)(0xC0 | (w >> 6));
            out[outIdx++] = (char)(0x80 | (w & 0x3F));
        }
        else {
            if (outIdx + 3 >= outSize) break;
            out[outIdx++] = (char)(0xE0 | (w >> 12));
            out[outIdx++] = (char)(0x80 | ((w >> 6) & 0x3F));
            out[outIdx++] = (char)(0x80 | (w & 0x3F));
        }
    }
    out[outIdx] = '\0';
}

int GetFontFamilyName(const char* path, char* outName, int outSize) {
    if (!path || !outName || outSize <= 1) return 0;
    outName[0] = '\0';

    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    unsigned char header[12];
    if (fread(header, 1, 12, f) != 12) { fclose(f); return 0; }

    uint32_t tag = ReadBE32(header);

    // "ttcf" 是 ttc，暂不支持
    if (tag == 0x74746366) { fclose(f); return 0; }

    // 0x00010000 (TrueType) 或 "OTTO" (OpenType)
    if (tag != 0x00010000 && tag != 0x4F54544F) {
        fclose(f);
        return 0;
    }

    uint16_t numTables = ReadBE16(header + 4);

    uint32_t nameOffset = 0;
    uint32_t nameLength = 0;

    unsigned char tableRec[16];
    for (int i = 0; i < numTables; ++i) {
        if (fread(tableRec, 1, 16, f) != 16) { fclose(f); return 0; }
        uint32_t t = ReadBE32(tableRec);
        if (t == 0x6E616D65) {   // "name"
            nameOffset = ReadBE32(tableRec + 8);
            nameLength = ReadBE32(tableRec + 12);
            break;
        }
    }

    if (nameOffset == 0 || nameLength < 6) { fclose(f); return 0; }

    if (fseek(f, nameOffset, SEEK_SET) != 0) { fclose(f); return 0; }

    unsigned char nameHeader[6];
    if (fread(nameHeader, 1, 6, f) != 6) { fclose(f); return 0; }

    uint16_t count = ReadBE16(nameHeader + 2);
    uint16_t stringOffset = ReadBE16(nameHeader + 4);

    unsigned char rec[12];
    int found = 0;

    for (int i = 0; i < count; ++i) {
        if (fread(rec, 1, 12, f) != 12) break;

        uint16_t platformID = ReadBE16(rec);
        uint16_t encodingID = ReadBE16(rec + 2);
        uint16_t nameID = ReadBE16(rec + 6);
        uint16_t length = ReadBE16(rec + 8);
        uint16_t offset = ReadBE16(rec + 10);

        // Family Name 的 nameID = 1
        if (nameID != 1) continue;

        int good = 0;
        if (platformID == 3 && encodingID == 1) good = 1;       // Windows Unicode
        else if (platformID == 1 && encodingID == 0) good = 2;  // Mac Roman

        if (!good) continue;

        if (fseek(f, (long)nameOffset + stringOffset + offset, SEEK_SET) != 0)
            continue;

        if (length == 0 || length > 512) continue;

        unsigned char buf[512];
        if (fread(buf, 1, length, f) != length) continue;

        if (good == 1) {
            UTF16BEToUTF8(buf, length, outName, outSize);
        }
        else {
            int copyLen = length;
            if (copyLen >= outSize) copyLen = outSize - 1;
            memcpy(outName, buf, copyLen);
            outName[copyLen] = '\0';
        }

        if (outName[0] != '\0') {
            found = 1;
            break;
        }
    }

    fclose(f);
    return found;
}