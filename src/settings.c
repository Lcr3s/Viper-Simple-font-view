#define _CRT_SECURE_NO_WARNINGS
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SETTINGS_FILE "settings.ini"

// 全局变量实际定义
char fontList[MAX_FONTS][512];
char fontNames[MAX_FONTS][128];
int  fontListCount = 0;
int  fontListSelected = -1;
float fontListScroll = 0.0f;

void SettingsLoad(Settings* s) {
    s->language = 0;
    s->dark = 0;
    s->uiFont[0] = '\0';

    FILE* f = fopen(SETTINGS_FILE, "r");
    if (!f) return;

    char line[600];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        if (strncmp(line, "language=", 9) == 0) {
            s->language = atoi(line + 9);
        }
        else if (strncmp(line, "dark=", 5) == 0) {
            s->dark = atoi(line + 5);
        }
        else if (strncmp(line, "uiFont=", 7) == 0) {
            strncpy(s->uiFont, line + 7, sizeof(s->uiFont) - 1);
            s->uiFont[sizeof(s->uiFont) - 1] = '\0';
        }
    }
    fclose(f);
}

void SettingsSave(const Settings* s) {
    FILE* f = fopen(SETTINGS_FILE, "w");
    if (!f) return;
    fprintf(f, "language=%d\n", s->language);
    fprintf(f, "dark=%d\n", s->dark);
    fprintf(f, "uiFont=%s\n", s->uiFont);
    fclose(f);
}