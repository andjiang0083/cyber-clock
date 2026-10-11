/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 andjiang0083
 */
// ============================================================
// core/input.h — Arcadia 统一输入抽象
// 屏蔽 S3 (BtnA/B) 和 Cardputer (键盘) 的差异
//
// 用法:
//   int key = arcGetKey();
//   if (key > 0) { /* key char */ }
//   else if (key == ARC_KEY_NONE) { /* 无按键 */ }
//
// S3 映射:
//   BtnA 短按 → 'A' (大写)
//   BtnA 长按 → 'a' (小写, 表示 hold)
//   BtnB 短按 → 'B'
//   BtnB 长按 → 'b'
//
// Cardputer 映射:
//   键盘直接输出字符
//   特殊键映射见 ARC_KEY_* 常量
// ============================================================
#pragma once
#include <cstdint>

// ===================== 特殊键常量 =====================
// 返回 > 0 为可打印字符, < 0 为特殊键
static const int ARC_KEY_NONE    = 0;     // 无按键
static const int ARC_KEY_UNKNOWN = -1;    // 未知按键

// ===================== 平台检测 =====================
#if defined(ARCADIA_CARDPUTER)
  #include <M5Cardputer.h>
  using namespace m5;
#else
  // 默认 S3
  #include <M5Unified.h>
#endif

// ===================== 获取按键 =====================
// 返回值:
//   0         = 无按键
//   >0 的char = 按键字符
//   <0        = 特殊键 (ARC_KEY_*)
static inline int arcGetKey() {
#if defined(ARCADIA_CARDPUTER)
    // ---- Cardputer: 键盘 ----
    M5Cardputer.update();
    if (!M5Cardputer.Keyboard.isChange()) return ARC_KEY_NONE;

    // 检查按键状态
    auto status = M5Cardputer.Keyboard.keysState();

    // 普通字符键
    for (auto key : status.word) {
        return (int)key;
    }

    // 特殊键 (退格/回车等)
    if (status.del)    return '\b';   // Backspace
    if (status.enter)  return '\n';   // Enter
    if (status.tab)    return '\t';   // Tab
    if (status.space)  return ' ';    // Space
    if (status.ctrl)   return -2;     // Ctrl
    if (status.alt)    return -3;     // Alt
    if (status.shift)  return -4;     // Shift
    if (status.fn)     return -5;     // Fn
    if (status.opt)    return -6;     // Option

    return ARC_KEY_NONE;

#else
    // ---- M5StickS3: 按钮 ----
    // 注意: 不要在这里调 M5.update() — 由调用者统一调用
    // 这里只做查询

    // 长按抑制: M5Unified 中 wasHold() 在按住超阈值那帧触发, wasClicked()
    // 在释放帧触发 — 一次长按会先后产生两个事件 (长按动作 + 短按动作)。
    // 这里在 hold 触发后 300ms 内吞掉 click, 长按只出 'a'/'b'。
    static uint32_t lastHoldMs = 0;
    uint32_t nowMs = millis();

    // BtnA 短按 (长按抑制窗口内不触发)
    if (M5.BtnA.wasClicked() && nowMs - lastHoldMs > 300) {
        return 'A';
    }
    // BtnA 长按
    if (M5.BtnA.wasHold()) {
        lastHoldMs = millis();
        return 'a';
    }
    // BtnB 短按
    if (M5.BtnB.wasClicked() && nowMs - lastHoldMs > 300) {
        return 'B';
    }
    // BtnB 长按
    if (M5.BtnB.wasHold()) {
        lastHoldMs = millis();
        return 'b';
    }

    // (可选) BtnPWR — 如果有
    if (M5.BtnPWR.wasClicked() && nowMs - lastHoldMs > 300) {
        return 'P';
    }

    return ARC_KEY_NONE;
#endif
}

// ===================== 等待释放 =====================
// 阻塞直到所有按键释放 (用于防连击)
static inline void arcWaitRelease() {
#if defined(ARCADIA_CARDPUTER)
    // Cardputer: 简单延时等待
    delay(50);
#else
    // S3: 等待 BtnA 和 BtnB 释放
    while (M5.BtnA.isPressed() || M5.BtnB.isPressed()) {
        M5.update();
        delay(5);
    }
#endif
}
