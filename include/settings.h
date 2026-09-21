#ifndef SETTINGS_H
#define SETTINGS_H

#define MAX_FONTS 512

typedef struct {
    int  language;       // 0 = 中文, 1 = English
    int  dark;           // 0 = 浅色, 1 = 夜间
    char uiFont[512];    // UI 字体路径（空 = 用默认）
} Settings;

// 全局变量声明 (在 settings.c 中定义)
extern char fontList[MAX_FONTS][512];
extern char fontNames[MAX_FONTS][128];
extern int  fontListCount;
extern int  fontListSelected;
extern float fontListScroll;

void SettingsLoad(Settings* s);
void SettingsSave(const Settings* s);

#endif