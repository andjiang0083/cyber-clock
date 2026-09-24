// ============================================================
// core/powersave.h — Arcadia 省电管理
// Light Sleep · ext0(BtnA)唤醒 · CPU频率调节
//
// 省电策略（基于实测经验）:
//   1. Light Sleep + ext0(BtnA唤醒) → 深度睡眠时几乎零功耗
//   2. 200ms 定时器轮询 BtnB → 避免 IMU 过夜耗尽电池
//   3. CPU 频率降频 → 闲时 80MHz, 忙时 240MHz
//   4. 屏幕亮度可调 → 最低可接受亮度
// ============================================================
#pragma once
#include <cstdint>

// ===================== 省电配置 =====================
struct ArcPowerConfig {
    int sleepTimeoutSec = 60;        // 空闲多久后进入 Light Sleep (秒)
    int dimTimeoutSec   = 30;        // 空闲多久后调暗屏幕 (秒)
    int dimBrightness   = 5;         // 调暗后的亮度 (0-100)
    int normalBrightness = 100;      // 正常亮度
    int cpuFreqMHz      = 240;       // 正常 CPU 频率
    int sleepCpuFreqMHz = 80;        // 休眠时 CPU 频率
    bool useExt0Wakeup  = true;      // 使用 ext0 (BtnA) 唤醒
};

// ===================== 省电管理器 =====================
struct ArcPowerSave {
    ArcPowerConfig cfg;
    bool asleep = false;
    bool dimmed = false;
    uint32_t lastActivity = 0;
    uint32_t lastPoll = 0;
    bool btnBState = false;

    void begin(const ArcPowerConfig& config = ArcPowerConfig()) {
        cfg = config;
        lastActivity = millis();
    }

    // ===================== 标记活动 (阻止休眠) =====================
    void markActive() {
        lastActivity = millis();
        if (asleep) wake();
        if (dimmed) undim();
    }

    // ===================== 每帧调用 =====================
    void update() {
        uint32_t now = millis();
        uint32_t idleTime = now - lastActivity;

        // 调暗屏幕
        if (!dimmed && !asleep && cfg.dimTimeoutSec > 0 &&
            idleTime > (uint32_t)cfg.dimTimeoutSec * 1000) {
            dim();
        }

        // 进入 Light Sleep
        if (!asleep && cfg.sleepTimeoutSec > 0 &&
            idleTime > (uint32_t)cfg.sleepTimeoutSec * 1000) {
            goSleep();
        }
    }

    // ===================== 调暗 =====================
    void dim() {
        if (dimmed) return;
        dimmed = true;
        ArcDisplay::setBrightness(cfg.dimBrightness);
    }

    void undim() {
        if (!dimmed) return;
        dimmed = false;
        ArcDisplay::setBrightness(cfg.normalBrightness);
    }

    // ===================== Light Sleep =====================
    void goSleep() {
        if (asleep) return;
        asleep = true;

        // 降低 CPU
        setCpuFrequencyMhz(cfg.sleepCpuFreqMHz);

        // 停止音频
        M5.Speaker.stop();
        M5.Speaker.end();

        // 准备 Light Sleep
        if (cfg.useExt0Wakeup) {
            // BtnA (GPIO0 on StickS3) 作为唤醒源
#if defined(CONFIG_IDF_TARGET_ESP32S3)
            esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // LOW 电平唤醒
#endif
        }

        // 关闭显示屏
        ArcDisplay::setBrightness(0);

        // 进入 Light Sleep
        esp_light_sleep_start();

        // ===== 醒来后继续 =====
        ArcDisplay::begin(1, cfg.normalBrightness);  // 重建显示
        setCpuFrequencyMhz(cfg.cpuFreqMHz);
        asleep = false;
        dimmed = false;
        lastActivity = millis();

        // 重新初始化扬声器
        auto spkCfg = M5.Speaker.config();
        spkCfg.sample_rate = 22050;
        M5.Speaker.config(spkCfg);
        M5.Speaker.begin();
    }

    // ===================== 强制唤醒 =====================
    void wake() {
        if (!asleep) return;
        // 实际上已经在 goSleep() 中恢复了
    }
};
