// ============================================================
// core/app.h — Arcadia 应用基类
// 生命周期管理 · 自动演示 · 省电 · WiFi
//
// 用法:
//   class MyApp : public ArcApp<4, 6> {  // 4个跟踪目标, 6个预设
//       void onSetup() override { ... }
//       void onLoop() override { ... }
//       void onKey(int key) override { ... }
//   } app;
//   void setup() { app.run(); }  // 不会返回
//   void loop() {}               // 不会执行到这里
// ============================================================
#pragma once
#include "display.h"
#include "input.h"
#include "screen.h"
#include "hud.h"
#include "audio.h"
#include "autodemo.h"
#include "powersave.h"

// ===================== 应用基类 =====================
// NTargets: 跟踪目标数 (自动演示用)
// NPresets: 预设场景数
template<int NTargets = 4, int NPresets = 6>
struct ArcApp {
    // ===================== 子系统 =====================
    ArcHUD hud;
    ArcAutoDemo autodemo{NTargets, NPresets};
    ArcPowerSave power;

    // ===================== 生命周期 (子类重写) =====================
    virtual const char* appName() = 0;
    virtual const char* appVersion() = 0;

    // 初始化
    virtual void onSetup() {}

    // 每帧
    virtual void onLoop() {}

    // 按键
    virtual void onKey(int /*key*/) {}

    // HUD (每帧渲染时调用)
    virtual void onHUD() {}

    // 自动演示预设切换时
    virtual void onPresetChange(int /*preset*/) {}

    // 暂停/恢复
    virtual void onPause() {}
    virtual void onResume() {}

    // ===================== Splash 控制 =====================
    virtual const char** splashControls() { return nullptr; }
    virtual int splashControlCount() { return 0; }
    virtual int splashDelayMs() { return 2500; }

    // ===================== 运行 (不会返回) =====================
    void run() {
        setup();
        while (true) {
            loop();
        }
    }

    // ===================== 内部 setup =====================
    void setup() {
        // 由子类自行调用 M5.begin() 和 ArcDisplay::begin()
        onSetup();

        // 自动演示
        autodemo.begin();

        // 省电
        power.begin();

        srand(millis());

        // Splash
        const char** ctrl = splashControls();
        int nCtrl = splashControlCount();
        if (ctrl && nCtrl > 0) {
            arcSplash(appName(), appVersion(), ctrl, nCtrl, ArcColor::SPLASH_BG, splashDelayMs());
        } else {
            arcSplash(appName(), appVersion(), nullptr, 0, ArcColor::SPLASH_BG, splashDelayMs());
        }
    }

    // ===================== 内部 loop =====================
    void loop() {
#if defined(ARCADIA_CARDPUTER)
        M5Cardputer.update();
#endif
        M5.update();

        // ===== 输入 =====
        int key = arcGetKey();
        if (key > 0) {
            arcClick();
            onKey(key);
            autodemo.onActivity();
            power.markActive();
        }

        // ===== 自动演示 =====
        autodemo.update();
        if (autodemo.didPresetChange()) {
            onPresetChange(autodemo.getPreset());
        }

        // ===== 省电 =====
        power.update();

        // ===== 应用逻辑 =====
        onLoop();

        // ===== 绘制 =====
        ArcDisplay::clear();

        // (子类的 onLoop 在这里负责核心绘制)

        // HUD
        hud.begin();
        onHUD();

        ArcDisplay::push();
        delay(16);
    }
};
