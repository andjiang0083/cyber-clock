/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 andjiang0083
 */
// ============================================================
// core/arcadia.h — Arcadia 框架一键包含
// 只需 #include 此文件即可使用全部GUI模块
//
// 用法:
//   #define ARCADIA_STICKS3    // 或 ARCADIA_CARDPUTER
//   #include <core/arcadia.h>
// ============================================================
#pragma once

// ===================== 包含基础 =====================
#include <Arduino.h>
#include <M5Unified.h>

// ===================== GUI 核心模块 =====================
#include "color.h"       // RGB565, 混合, HSV, 色板
#include "display.h"     // Canvas, 旋转, 推屏
#include "input.h"       // 统一输入 (S3按钮 / Cardputer键盘)
#include "screen.h"      // Splash, 帮助覆盖层, 过渡
#include "hud.h"         // HUD 文本行
#include "audio.h"       // 音效, 摩斯引擎

// ===================== 可选模块 (按需) =====================
// 3D 引擎
#include "math3d.h"      // V3, M3, 旋转, 投影
#include "stars.h"       // 3D星场
#include "imu.h"         // IMU 互补滤波

// Phase 2: 应用运行时
#include "autodemo.h"    // 空闲检测 + 自动演示
#include "powersave.h"   // Light Sleep 省电
#include "app.h"         // 应用基类 (生命周期)

// 实用工具 (按需 #include)
// #include "wifi_mgr.h" // WiFi配网 (依赖 WiFi.h + WebServer)
