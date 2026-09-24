// ============================================================
// core/autodemo.h — Arcadia 自动演示系统
// 空闲检测 → 自动演示 → 相机漂移 → 场景切换
//
// 从 threebody 项目提取的核心模式:
//   1. 空闲 N 秒 → 进入自动演示模式
//   2. 相机自动旋转 + 多正弦波叠加随机漂移 (永不重复)
//   3. 循环切换跟踪目标 + 预设场景
//   4. 任意按键退出自动演示
// ============================================================
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdlib>

// ===================== 自动演示配置 =====================
struct ArcAutoDemoConfig {
    uint32_t idleTimeoutMs = 15000;       // 空闲超时 (ms)
    uint32_t trackCycleMs  = 10000;       // 跟踪切换周期 (ms)
    uint32_t presetCycleMs = 50000;       // 预设切换周期 (ms)
    float autoRotSpeed     = 0.0025f;     // 自动旋转速度 (rad/frame)
    float demoRotSpeed     = 0.0015f;     // 演示模式旋转速度 (更慢)
    float swayAmplitude    = 0.20f;       // 相机漂移幅度
};

// ===================== 自动演示管理器 =====================
// 用法:
//   ArcAutoDemo demo(4, 6);  // 4个跟踪目标, 6个预设
//   demo.onActivity();       // 任何按键/触摸后调用
//   demo.update();           // 每帧调用
//   if (demo.isActive()) { ... }  // 是否在演示中
struct ArcAutoDemo {
    int _nTargets;           // 跟踪目标数 (如 4: COM, 体0, 体1, 体2)
    int _nPresets;           // 预设场景数 (如 6)
    int _currentTarget = 0;
    int _currentPreset = 0;
    float _autoAng = 0;      // 自动旋转角度
    float _swayX = 0;        // 相机漂移 X (上下摇摆)
    float _swayY = 0;        // 相机漂移 Y (左右摇摆)
    float _simTime = 0;      // 模拟时间 (用于漂移波形)
    bool _active = false;    // 是否在演示模式
    uint32_t _idleStart = 0;
    uint32_t _lastTrackSwitch = 0;
    uint32_t _lastPresetSwitch = 0;
    ArcAutoDemoConfig cfg;

    ArcAutoDemo(int nTargets = 4, int nPresets = 6)
        : _nTargets(nTargets), _nPresets(nPresets) {}

    void begin(const ArcAutoDemoConfig& config = ArcAutoDemoConfig()) {
        cfg = config;
        _idleStart = millis();
        _lastTrackSwitch = millis();
        _lastPresetSwitch = millis();
    }

    // ===================== 标记活动 =====================
    void onActivity() {
        if (_active) {
            // 任意按键退出自动演示
            _active = false;
            _swayX = 0;
            _swayY = 0;
        }
        _idleStart = millis();
    }

    // ===================== 设置模拟时间 (用于漂移) =====================
    void setSimTime(float t) { _simTime = t; }

    // ===================== 每帧更新 =====================
    void update() {
        uint32_t now = millis();
        float t = _simTime * 0.001f;

        // 空闲检测
        if (!_active && (now - _idleStart > cfg.idleTimeoutMs)) {
            _active = true;
            _currentTarget = 0;
            _lastTrackSwitch = now;
            _lastPresetSwitch = now;
        }

        // 自动旋转
        _autoAng += _active ? cfg.demoRotSpeed : cfg.autoRotSpeed;
        if (_autoAng > 2.0f * M_PI) _autoAng -= 2.0f * M_PI;

        if (_active) {
            // 相机漂移: 多组正弦波叠加, 永不重复
            _swayX = cfg.swayAmplitude * (
                1.0f * sinf(t * 0.47f + 0.3f) +
                0.6f * sinf(t * 1.13f + 1.7f) +
                0.3f * sinf(t * 2.71f + 4.2f)
            );
            _swayY = cfg.swayAmplitude * 0.75f * (
                1.0f * sinf(t * 0.53f + 2.1f) +
                0.66f * sinf(t * 0.97f + 0.8f) +
                0.33f * sinf(t * 2.23f + 3.5f)
            );

            // 跟踪切换
            if (now - _lastTrackSwitch > cfg.trackCycleMs) {
                int prev = _currentTarget;
                while (_currentTarget == prev && _nTargets > 1)
                    _currentTarget = rand() % _nTargets;
                _lastTrackSwitch = now;
            }

            // 预设切换 (返回 true 表示需要重新初始化场景)
            // 调用者检查 didPresetChange()
        }
    }

    // ===================== 检查预设是否改变 =====================
    bool didPresetChange() {
        if (!_active) return false;
        uint32_t now = millis();
        if (now - _lastPresetSwitch > cfg.presetCycleMs) {
            int prev = _currentPreset;
            while (_currentPreset == prev && _nPresets > 1)
                _currentPreset = rand() % _nPresets;
            _lastPresetSwitch = now;
            _lastTrackSwitch = now;
            _currentTarget = rand() % _nTargets;
            return true;
        }
        return false;
    }

    // ===================== 查询状态 =====================
    bool isActive() const { return _active; }
    float getAutoAngle() const { return _autoAng; }
    float getSwayX() const { return _swayX; }
    float getSwayY() const { return _swayY; }
    int getTarget() const { return _currentTarget; }
    int getPreset() const { return _currentPreset; }

    // ===================== 获取速度倍率 (演示模式时变化) =====================
    float getSpeedMul() const {
        if (!_active) return 1.0f;
        return 0.7f + 0.8f * (0.5f + 0.5f * sinf(millis() * 0.00002f + sinf(millis() * 0.0001f) * 0.5f));
    }
};
