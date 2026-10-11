/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 andjiang0083
 */
// ============================================================
// core/hud.h — Arcadia HUD (Heads-Up Display) 系统
// 文本行管理 · 颜色主题 · 快捷 printf
//
// 用法:
//   ArcHUD hud;
//   hud.begin(2, ArcColor::HUD_MAIN);
//   hud.line("t=%.1f [COM]", simTime);
//   hud.line("E=%.2f", energy);
//   hud.setColor(ArcColor::HUD_DIM);
//   hud.line("z=x%.1f", zoom);
// (每帧重新绘制前调用 hud.reset())
// ============================================================
#pragma once
#include "display.h"
#include "color.h"

struct ArcHUD {
    int _x = 2;
    int _y = 2;
    int _lineH = 10;      // 行高 (Font0 size1 ≈ 10px)
    int _yStart = 2;      // 起始 Y
    uint16_t _color = ArcColor::HUD_MAIN;
    uint16_t _bg = TFT_BLACK;

    // ===================== 开始 =====================
    void begin(int startY = 2, uint16_t color = ArcColor::HUD_MAIN) {
        _yStart = startY;
        _y = startY;
        _color = color;
        _bg = TFT_BLACK;
        auto& c = ArcDisplay::canvas;
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
    }

    // ===================== 重置到起始行 =====================
    void reset() {
        _y = _yStart;
    }

    // ===================== 设置颜色 =====================
    void setColor(uint16_t col) { _color = col; }

    // ===================== 设置行高 =====================
    void setLineH(int h) { _lineH = h; }

    // ===================== 输出一行文本 =====================
    void line(const char* s) {
        auto& c = ArcDisplay::canvas;
        c.setTextColor(_color, _bg);
        c.setCursor(_x, _y);
        c.print(s);
        _y += _lineH;
    }

    // ===================== 格式化输出一行 =====================
    void linef(const char* fmt, ...) {
        char buf[64];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        line(buf);
    }

    // ===================== 直接定位输出 =====================
    void at(int x, int y, const char* s) {
        auto& c = ArcDisplay::canvas;
        c.setTextColor(_color, _bg);
        c.setCursor(x, y);
        c.print(s);
    }

    // ===================== 清除 HUD 区域 =====================
    void clearArea(int x, int y, int w, int h) {
        ArcDisplay::fillRect(x, y, w, h, _bg);
    }

    // ===================== 右侧缩略指示 =====================
    // 在右上角显示一个小标记, 如 "[M]" 代表静音
    void indicator(const char* tag, uint16_t col = ArcColor::HUD_ALERT) {
        auto& c = ArcDisplay::canvas;
        c.setTextColor(col, _bg);
        c.setCursor(ArcDisplay::W - 30, _yStart);
        c.print(tag);
    }
};
