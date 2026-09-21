#pragma once
#include <raylib.h>

typedef struct {
    Color bg;
    Color text;
    Color textDim;
    Color border;
    Color borderActive;
    Color btnHover;
    Color inputBg;
    Color listSel;
    Color scrollTrack;
    Color scrollThumb;
    Color scrollThumbHover;
    Color panelBg;
    Color switchBgOff;
    Color switchBgOn;
    Color switchKnob;
} Theme;

extern const Theme themeLight;
extern const Theme themeDark;

Color LerpColor(Color a, Color b, float t);
Theme BlendTheme(float t, const Theme* light, const Theme* dark);