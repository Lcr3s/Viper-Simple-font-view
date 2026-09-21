#include "theme.h"

const Theme themeLight = {
    {245, 245, 245, 255},
    {40, 40, 40, 255},
    {130, 130, 130, 255},
    {80, 80, 80, 255},
    {0, 120, 215, 255},
    {230, 230, 230, 255},
    {240, 240, 240, 255},
    {220, 230, 255, 255},
    {240, 240, 240, 255},
    {180, 180, 180, 255},
    {130, 130, 130, 255},
    {252, 252, 252, 255},
    {200, 200, 200, 255},
    {80, 160, 240, 255},
    {255, 255, 255, 255},
};

const Theme themeDark = {
    {30, 30, 30, 255},
    {220, 220, 220, 255},
    {130, 130, 130, 255},
    {120, 120, 120, 255},
    {80, 160, 240, 255},
    {60, 60, 60, 255},
    {45, 45, 45, 255},
    {50, 70, 110, 255},
    {45, 45, 45, 255},
    {90, 90, 90, 255},
    {130, 130, 130, 255},
    {40, 40, 40, 255},
    {80, 80, 80, 255},
    {80, 160, 240, 255},
    {230, 230, 230, 255},
};

Color LerpColor(Color a, Color b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color) {
        (unsigned char)(a.r + (b.r - a.r) * t),
            (unsigned char)(a.g + (b.g - a.g) * t),
            (unsigned char)(a.b + (b.b - a.b) * t),
            (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

Theme BlendTheme(float t, const Theme* light, const Theme* dark) {
    Theme r;
    r.bg = LerpColor(light->bg, dark->bg, t);
    r.text = LerpColor(light->text, dark->text, t);
    r.textDim = LerpColor(light->textDim, dark->textDim, t);
    r.border = LerpColor(light->border, dark->border, t);
    r.borderActive = LerpColor(light->borderActive, dark->borderActive, t);
    r.btnHover = LerpColor(light->btnHover, dark->btnHover, t);
    r.inputBg = LerpColor(light->inputBg, dark->inputBg, t);
    r.listSel = LerpColor(light->listSel, dark->listSel, t);
    r.scrollTrack = LerpColor(light->scrollTrack, dark->scrollTrack, t);
    r.scrollThumb = LerpColor(light->scrollThumb, dark->scrollThumb, t);
    r.scrollThumbHover = LerpColor(light->scrollThumbHover, dark->scrollThumbHover, t);
    r.panelBg = LerpColor(light->panelBg, dark->panelBg, t);
    r.switchBgOff = LerpColor(light->switchBgOff, dark->switchBgOff, t);
    r.switchBgOn = LerpColor(light->switchBgOn, dark->switchBgOn, t);
    r.switchKnob = LerpColor(light->switchKnob, dark->switchKnob, t);
    return r;
}