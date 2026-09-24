// ============================================================
// core/screen.h — Arcadia 画面系统
// Splash 启动画面 · 帮助覆盖层 · 过渡效果
// ============================================================
#pragma once
#include "display.h"
#include "color.h"

// ===================== Splash 启动画面 =====================
// 显示: [背景] [标题+版本] [操作提示] → 延时后自动清除
// title:    应用名 (如 "Three-Body")
// version:  版本号 (如 "v2.4.0")
// controls: 操作说明行 (如 {"A=Cam", "B=Preset", ...})
// nControls: 说明行数
// bgColor:  背景色 (默认深空蓝)
// delayMs:  显示时长 (默认 2500ms)
static inline void arcSplash(
    const char* title,
    const char* version,
    const char* controls[] = nullptr,
    int nControls = 0,
    uint16_t bgColor = ArcColor::SPLASH_BG,
    int delayMs = 2500
) {
    auto& c = ArcDisplay::canvas;
    int cx = ArcDisplay::CX, cy = ArcDisplay::CY;

    // 背景
    c.fillScreen(bgColor);
    c.setFont(&fonts::Font0);

    // 版本号 (大字)
    c.setTextSize(2);
    c.setTextColor(ArcColor::SPLASH_TITLE, bgColor);
    int titleW = strlen(version) * 12;  // Font0 size2 ≈ 12px/char
    c.setCursor(cx - titleW / 2, 10);
    c.print(version);

    // 应用名 (中字)
    c.setTextSize(1);
    c.setTextColor(ArcColor::SPLASH_TITLE, bgColor);
    int nameW = strlen(title) * 6;
    c.setCursor(cx - nameW / 2, 38);
    c.print(title);

    // 操作说明 (小字, 底部)
    if (controls && nControls > 0) {
        c.setTextSize(1);
        c.setTextColor(ArcColor::SPLASH_HINT, bgColor);
        for (int i = 0; i < nControls; i++) {
            c.setCursor(4, cy + 20 + i * 12);
            c.print(controls[i]);
        }
    }

    // 推屏
    ArcDisplay::push();

    // 等待 (可跳过 — 任意按键退出)
    uint32_t start = millis();
    while (millis() - start < (uint32_t)delayMs) {
#if defined(ARCADIA_CARDPUTER)
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) break;
#else
        M5.update();
        if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked()) break;
#endif
        delay(10);
    }

    // 清除
    ArcDisplay::clear();
    ArcDisplay::push();
}

// ===================== 帮助覆盖层 =====================
// 在半透明黑底上显示帮助文本行
// lines: 文本行数组, 以 nullptr 结尾 (或空行 "")
static inline void arcHelpOverlay(
    const char* title,
    const char* lines[],
    uint16_t titleColor = c565(200, 200, 255),
    uint16_t textColor = c565(180, 180, 200)
) {
    auto& c = ArcDisplay::canvas;
    int w = ArcDisplay::W, h = ArcDisplay::H;

    // 填充半透明黑底 (不透明度由 fillRect 模拟)
    c.fillRect(0, 0, w, h, c565(0, 0, 0));
    c.setFont(&fonts::Font0);
    c.setTextSize(1);

    int y = 6;
    auto p = [&](const char* s, uint16_t col) {
        c.setTextColor(col, c565(0, 0, 0));
        c.setCursor(4, y);
        c.print(s);
        y += 11;
    };

    // 标题
    p(title, titleColor);
    y += 3;

    // 行
    for (int i = 0; lines[i] != nullptr && lines[i][0] != '\0'; i++) {
        p(lines[i], textColor);
    }

    ArcDisplay::push();
}

// ===================== 过渡淡入/淡出 =====================
static inline void arcFadeIn(int steps = 8, int stepMs = 30) {
    for (int i = 0; i <= steps; i++) {
        int b = (i * 255) / steps;
        if (b > 100) b = 100;  // 上限
        ArcDisplay::setBrightness(b);
        delay(stepMs);
    }
}

static inline void arcFadeOut(int steps = 8, int stepMs = 30) {
    for (int i = steps; i >= 0; i--) {
        int b = (i * 255) / steps;
        if (b > 100) b = 100;
        ArcDisplay::setBrightness(b);
        delay(stepMs);
    }
}
