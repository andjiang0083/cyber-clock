// ============================================================
// config.h — CYBER CLOCK 配置参数
// 所有可调参数集中管理
// ============================================================
#pragma once

#include <cstdint>

// ── 版本 (splash 显示) ──
#define VERSION "v1.1.1"

// ── 显示 ──
static const int SCREEN_ROTATION = 1;
static const int BRIGHTNESS_DEFAULT_IDX = 3;  // 默认30%

// ── 亮度档位 (7级) ──
static const uint8_t BRIGHTNESS_LEVELS[] = { 3, 13, 26, 76, 128, 204, 255 };
static const uint8_t BRIGHTNESS_PCT[]   = { 1,  5, 10, 30,  50,  80, 100 };
static const int BRIGHTNESS_N = sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);

// ── 省电 ──
static const int IDLE_POWERSAVE_MS  =  60000;  // 60s → CPU 80MHz
static const int IDLE_SLEEP_MS      = 180000;  // 180s → Light Sleep
static const int SLEEP_POLL_US      = 200000;  // 200ms 定时唤醒轮询

// ── 矩阵雨 ──
static const int MATRIX_DROPS = 55;
static const char MATRIX_CHARS[] = "01#*+:.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz<>/|~@%&$";

// ── 音频 ──
static const int AUDIO_BARS       = 40;
static const int AUDIO_SAMPLES    = 160;
static const int AUDIO_INTERVAL_MS = 50;

// ── 抖动检测 ──
static const float SHAKE_THRESHOLD = 0.15f;
static const int SHAKE_COUNT_TRIGGER = 3;

// ── IMU ──
static const float IMU_TAU = 1.0f;

// ── NTP ──
static const int NTP_RETRIES = 20;
static const int NTP_RETRY_MS = 250;
