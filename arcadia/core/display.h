/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 andjiang0083
 */
// ============================================================
// core/display.h — Arcadia 显示管理器
// Canvas 双缓冲 · 旋转 · 亮度 · 尺寸
//
// 注意: M5StickS3 支持两种屏幕方向:
//   Rotation 0/2 → 竖屏 135×240 (默认, 按钮在下)
//   Rotation 1/3 → 横屏 240×135 (按钮在右/左)
// 开发新应用时, 先确定目标方向再调用 begin()
// ============================================================
#pragma once
#include <M5Unified.h>

namespace ArcDisplay {

// ===================== 全局状态 =====================
static int CX = 0, CY = 0, W = 0, H = 0;
static lgfx::LGFX_Sprite canvas(&M5.Display);
static bool initialized = false;

// ===================== 初始化 =====================
// rotation: 屏幕方向
//   M5StickS3:
//     0 → 竖屏 135×240 (默认, 按钮在下)
//     1 → 横屏 240×135 (按钮在右)
//     2 → 竖屏 135×240 (倒转)
//     3 → 横屏 240×135 (按钮在左)
//   Cardputer:
//     0 → 竖屏 135×240 (默认)
//     1 → 横屏 240×135 (常用)
// brightness: 0-255
static inline void begin(int rotation = 1, int brightness = 100) {
    if (initialized) return;
    M5.Display.setRotation(rotation);
    M5.Display.setBrightness(brightness);
    W = M5.Display.width();
    H = M5.Display.height();
    CX = W / 2;
    CY = H / 2;
    canvas.setColorDepth(16);
    canvas.createSprite(W, H);
    initialized = true;
}

// ===================== 清屏 =====================
static inline void clear(uint16_t color = TFT_BLACK) {
    canvas.fillSprite(color);
}

// ===================== 推屏 =====================
static inline void push() {
    canvas.pushSprite(0, 0);
}

// ===================== 便捷绘制 (封装常用) =====================
static inline void pixel(int x, int y, uint16_t col) {
    if (x >= 0 && x < W && y >= 0 && y < H)
        canvas.drawPixel(x, y, col);
}

static inline void hline(int x, int y, int w, uint16_t col) {
    canvas.drawFastHLine(x, y, w, col);
}

static inline void vline(int x, int y, int h, uint16_t col) {
    canvas.drawFastVLine(x, y, h, col);
}

static inline void rect(int x, int y, int w, int h, uint16_t col) {
    canvas.drawRect(x, y, w, h, col);
}

static inline void fillRect(int x, int y, int w, int h, uint16_t col) {
    canvas.fillRect(x, y, w, h, col);
}

static inline void circle(int cx, int cy, int r, uint16_t col) {
    canvas.drawCircle(cx, cy, r, col);
}

static inline void fillCircle(int cx, int cy, int r, uint16_t col) {
    canvas.fillCircle(cx, cy, r, col);
}

static inline void line(int x1, int y1, int x2, int y2, uint16_t col) {
    canvas.drawLine(x1, y1, x2, y2, col);
}

// ===================== 文本便捷 =====================
static inline void text(const char* s, int x, int y, uint16_t col, uint16_t bg = TFT_BLACK) {
    canvas.setTextColor(col, bg);
    canvas.setCursor(x, y);
    canvas.print(s);
}

static inline void printf(int x, int y, uint16_t col, uint16_t bg, const char* fmt, ...) {
    canvas.setTextColor(col, bg);
    canvas.setCursor(x, y);
    va_list args;
    va_start(args, fmt);
    canvas.printf(fmt, args);
    va_end(args);
}

// ===================== 亮度控制 =====================
static inline void setBrightness(uint8_t b) {
    M5.Display.setBrightness(b);
}

// ===================== 释放资源 =====================
static inline void end() {
    canvas.deleteSprite();
    initialized = false;
}

} // namespace ArcDisplay
