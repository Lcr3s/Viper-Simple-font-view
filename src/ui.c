#include "ui.h"

void DrawIcon(Texture2D tex, Rectangle box) {
    if (tex.id == 0) return;
    float pad = 6.0f;   // 图标内边距
    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { box.x + pad, box.y + pad,
                      box.width - pad * 2, box.height - pad * 2 };
    DrawTexturePro(tex, src, dst, (Vector2) { 0, 0 }, 0, WHITE);
}