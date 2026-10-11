/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 andjiang0083
 */
// ============================================================
// core/color.h — Arcadia 颜色工具
// RGB565 转换 · 混合 · HSV · 随机
// 提取自 threebody/render_helpers.h, 全项目共用
// ============================================================
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdlib>

// ===================== RGB565 构造 =====================
// 8:8:8 → 5:6:5 (R高位,G中位,B低位)
static inline uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// ===================== 16位色分解 =====================
static inline uint8_t c565_r(uint16_t c) { return (c >> 11) & 0x1F; }
static inline uint8_t c565_g(uint16_t c) { return (c >> 5)  & 0x3F; }
static inline uint8_t c565_b(uint16_t c) { return c & 0x1F; }

// ===================== HSV → RGB565 =====================
// h: 0-360, s/v: 0-255
static inline uint16_t hsv2rgb(int h, uint8_t s, uint8_t v) {
    h %= 360; if (h < 0) h += 360;
    uint8_t region = h / 60;
    uint8_t f = h % 60;
    uint8_t p = (uint8_t)((uint16_t)v * (255 - s) / 255);
    uint8_t q = (uint8_t)((uint16_t)v * (255 - (uint16_t)s * f / 60) / 255);
    uint8_t t = (uint8_t)((uint16_t)v * (255 - (uint16_t)s * (60 - f) / 60) / 255);
    uint8_t r, g, b;
    switch (region) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        default: r=v; g=p; b=q; break;
    }
    return c565(r, g, b);
}

// ===================== 颜色混合 =====================
// lerp between two RGB565 colors, t=[0,1]
static inline uint16_t blend565(uint16_t c1, uint16_t c2, float t) {
    if (t <= 0.0f) return c1;
    if (t >= 1.0f) return c2;
    int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
    return (r << 11) | (g << 5) | b;
}

// ===================== 随机工具 =====================
static inline float frnd() { return (float)rand() / (float)RAND_MAX; }

// ===================== 预设色板 =====================
// 通用的暗色调 HUD 颜色 (在纯黑背景上显示)
namespace ArcColor {
    // HUD 主色调
    static const uint16_t HUD_MAIN   = c565(140, 180, 240);  // 淡蓝
    static const uint16_t HUD_SUB    = c565(100, 140, 190);  // 暗蓝
    static const uint16_t HUD_DIM    = c565(80, 80, 80);     // 深灰
    static const uint16_t HUD_ALERT  = c565(200, 70, 70);    // 红
    static const uint16_t HUD_OK     = c565(60, 180, 100);   // 绿
    static const uint16_t HUD_ACCENT = c565(150, 180, 255);  // 亮蓝白

    // Splash 标题色
    static const uint16_t SPLASH_TITLE = c565(150, 180, 255);
    static const uint16_t SPLASH_HINT  = c565(50, 70, 100);
    static const uint16_t SPLASH_BG    = c565(2, 2, 15);

    // 三体风格配色 (示例: 用于3D星体等)
    namespace ThreeBody {
        static const uint16_t BODY_A = c565(255, 70,  50);   // 红橙
        static const uint16_t BODY_B = c565(50,  220, 255);  // 青
        static const uint16_t BODY_C = c565(200, 80,  255);  // 紫
    }
}
