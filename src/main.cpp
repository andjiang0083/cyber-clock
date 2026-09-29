// ────────────────────────────────────────────────────────────
// CYBER CLOCK — M5StickS3 赛博朋克桌面时钟
// ────────────────────────────────────────────────────────────

#include <core/arcadia.h>
#include <time.h>
#include <Preferences.h>

#include "config.h"
#include "timesync.h"
#include "wifisetup.h"

using namespace ArcDisplay;

// ── Theme ──
struct Theme {
  uint16_t main, accent, date, status, rain, scan;
  const char* name;
};
static Theme THEMES[6];
static int g_theme = 0;

// ── Density modes ──
enum class Density : uint8_t { FULL, TIME, SAVER };
static Density g_density = Density::FULL;

// ── Time state ──
static int g_h = 0, g_m = 0, g_s = 0;
static int g_day = 27, g_mon = 6, g_year = 2026, g_wday = 6;
time_t g_epoch = 0;
uint32_t g_epochBase = 0;
bool g_timeValid = false;

// ── Matrix rain modes ──
// 注意: 成员避开 HEX — Arduino Print.h 有 #define HEX 16, 会宏展开破坏枚举。
enum class RainMode : uint8_t { MATRIX, NUMBER, BINARY, HEXADECIMAL, SYMBOL, ALPHA, MODES_N };
static RainMode g_rainMode = RainMode::MATRIX;

static const char* RAIN_CHARSETS[(int)RainMode::MODES_N] = {
  MATRIX_CHARS,                          // Matrix (复用 config.h 字符集)
  "0123456789",          // 数字雨
  "01",                  // 二进制
  "0123456789ABCDEF",    // 十六进制
  "#*+:.<>/|~@%&$",      // 符号
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ", // 字母
};
// ── Matrix rain (column-drop) ──
static const int TRAIL_LEN = 5;
#define MATRIX_N MATRIX_DROPS
static struct {
  float x, y, spd;
  char headCh;
  uint8_t headBri;
  char trail[TRAIL_LEN];
} g_rain[MATRIX_N];

// ── IMU ──
static ArcIMU g_imu;
static float g_prevP = 0, g_prevR = 0;
static int g_shakeCnt = 0;

// ── Audio state ──
static bool g_micActive = false;
static bool g_micFailed = false;
static uint8_t g_audioBars[AUDIO_BARS] = {0};
static uint8_t g_audioTargets[AUDIO_BARS] = {0};
static uint32_t g_audioLastSample = 0;
static int16_t g_micBuf[AUDIO_SAMPLES];

// ── Brightness ──
static int g_brightnessIdx = BRIGHTNESS_DEFAULT_IDX;

// ── Power save ──
static uint32_t g_lastActivity = 0;
static bool g_powerSave = false;
static bool g_extremeSave = false;
static int g_lastRenderSec = -1;

// ── Timezone (NVS persisted) ──
int g_tzOffset = 8;
static bool g_tzConfigured = false;

// ── Function declarations ──
static void initThemes();
static void initMatrixRain();
static void updateMatrixRain();
static void drawMatrixRain();
static void updateTime();
static void drawTime();
static void drawDate();
static void drawBattery();
static void initAudio();
static void stopAudio();
static void updateAudio();
static void drawAudioBars();
static void render(Density density);
static void updateIMU();
static void checkShake();
static void setBrightness(int idx);
static void pickTimezone();
static int tzLoad();
static void handleButtons();
static RainMode nextRainMode(RainMode m);

// ── USB 供电判定 (缓存 1s) ──
// 不要用 M5.Power.isCharging(): 它读充电器的 STAT 脚, 充满后立刻变 false,
// 插着线的桌面时钟会被误判成"没插电"而进省电 → 背光归零。VBUS 电压才是"插着线"。
static bool g_usbPlugged = false;
static uint32_t g_usbLastRead = 0;
static bool usbPowerPresent() {
  uint32_t now = millis();
  if (now - g_usbLastRead >= 1000) {
    g_usbLastRead = now;
    g_usbPlugged = M5.Power.getVBUSVoltage() > 1000;  // mV
  }
  return g_usbPlugged;
}

// ── Setup ──
void setup() {
  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::board_M5StickS3;
  M5.begin(cfg);

  // 随机种子: 数字雨每次开机序列不同
  srand(esp_random());

  // Landscape 240×135
  ArcDisplay::begin(SCREEN_ROTATION, BRIGHTNESS_LEVELS[g_brightnessIdx]);

  // sprite 分配失败 → canvas 上所有绘制会静默失效: 整屏黑、不崩溃、日志也空白
  // (docs/bugfix-black-screen-spec.md)。这里显式熔断, 把黑屏变成一条可读信息。
  if (!ArcDisplay::canvas.getBuffer()) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(12, 58);
    M5.Display.print("SPRITE FAIL");
    while (true) delay(1000);
  }

  // Init themes after M5.begin
  initThemes();

  // Init matrix rain
  initMatrixRain();

  // ── Custom splash with matrix rain background ──
  {
    // Run matrix rain for 3s with overlaid text
    uint32_t splashStart = millis();
    while (millis() - splashStart < 3000) {
      updateMatrixRain();
      canvas.startWrite();
      clear(TFT_BLACK);
      drawMatrixRain();

      // 半透明黑色遮罩条 (让文字可读)
      canvas.fillRect(0, 20, 240, 55, c565(0, 0, 0));

      // 标题 (大号, 居中)
      canvas.setTextSize(2);
      canvas.setTextColor(c565(80, 255, 120), c565(0, 0, 0));
      int tw = strlen("CYBER CLOCK") * 12;
      canvas.setCursor((240 - tw) / 2, 24);
      canvas.print("CYBER CLOCK");

      // 版本 (小号, 居中)
      canvas.setTextSize(1);
      canvas.setTextColor(c565(150, 200, 160), c565(0, 0, 0));
      tw = strlen(VERSION) * 6;
      canvas.setCursor((240 - tw) / 2, 50);
      canvas.print(VERSION);

      // 按键说明 (底部) — 启动期与运行期分开写: 启动画面上按住 A 是进配网,
      // 旧文案 "Hold-A=RainFX" 会把人骗进配网流程 (StickS3 没有 W 键, 这里也是唯一提示位)
      canvas.setTextSize(1);
      canvas.setTextColor(c565(140, 200, 160), TFT_BLACK);
      canvas.setCursor(8, 110);
      canvas.print("Tap=skip  Hold-A=WiFi setup");
      canvas.setCursor(8, 122);
      canvas.print("A=Screen B=Theme Hold=Rain/Bright");

      canvas.endWrite();
      push();

      // 可跳过
      M5.update();
      if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked()) break;
      delay(10);
    }
    ArcDisplay::clear();
    ArcDisplay::push();
  }

  // Load timezone from NVS
  int savedTz = tzLoad();
  if (savedTz != 999) { g_tzOffset = savedTz; g_tzConfigured = true; }

  // Check if BtnA held during splash to force WiFi reconfig
  M5.update();
  bool forceSetup = M5.BtnA.isPressed();

  if (forceSetup) {
    // Unified: TZ picker + WiFi reconfig
    pickTimezone();
    wifiSetupBegin(true);
    wifiStopAll();
    g_lastActivity = millis();
  } else {
    bool wifiOk = wifiSetupBegin(false);
    if (wifiOk && !g_tzConfigured) {
      pickTimezone();
      syncNTP(); // re-sync with chosen TZ — 必须在关射频之前
    }
    // 成功/失败/取消三条路径都要关射频 + 复位空闲计时。
    // 旧代码只在 wifiSetupBegin() 返回 true 时关: 取消配网/扫不到网络会一直开着 WiFi 射频。
    wifiStopAll();
    g_lastActivity = millis();
  }

  // Start audio capture (for bottom waveform bars)
  // Falls back silently if mic not available.
  initAudio();
}

// ── Loop ──
void loop() {
  uint32_t now = millis();

  // ── Power save state machine ──
  // 插着 USB 就跳过省电 (否则插线充电到 100% 后会自己黑屏)
  bool plugged = usbPowerPresent();
  if (plugged) {
    // Wake from power save if USB plugged
    if (g_powerSave || g_extremeSave) {
      g_powerSave = false;
      g_extremeSave = false;
      setCpuFrequencyMhz(240);
      ArcDisplay::setBrightness(BRIGHTNESS_LEVELS[g_brightnessIdx]);
      pinMode(24, OUTPUT);
      digitalWrite(24, LOW);
      g_lastRenderSec = -1;
      g_lastActivity = now;
      g_micFailed = false;
      initAudio();
    }
  }

  // 前置条件不再要求 g_timeValid: 没配上 WiFi 的设备同样要省电 (旧逻辑下会常亮到没电)
  if (!g_powerSave && now - g_lastActivity > IDLE_POWERSAVE_MS && !plugged) {
    g_powerSave = true;
    setCpuFrequencyMhz(80);
    stopAudio();
  }
  if (!g_extremeSave && now - g_lastActivity > IDLE_SLEEP_MS && !plugged) {
    // Enter extreme power save
    g_extremeSave = true;
    M5.Display.setBrightness(0);
    pinMode(24, OUTPUT);
    digitalWrite(24, HIGH); // try to turn off green LED (active low)
    stopAudio();
  }

  M5.update();
  updateIMU();
  updateTime();
  checkShake();

  // ── Input via Arcadia ──
  {
    int key = arcGetKey();
    bool pressed = false;

    if (key == 'A') {         // BtnA 短按 → 界面循环
      g_density = (Density)(((int)g_density + 1) % 3);
      pressed = true;
    }
    if (key == 'a') {         // BtnA 长按 → 切换数字雨效果 (任何界面)
      g_rainMode = nextRainMode(g_rainMode);
      pressed = true;
    }
    if (key == 'B') {         // BtnB 短按 → 按界面切换不同功能
      if (g_density == Density::SAVER) {
        // "数字雨"界面: 切换数字雨效果 (雨继续下, 不重置)
        g_rainMode = nextRainMode(g_rainMode);
      } else {
        // "完整界面"或"时钟+数字雨": 切换主题色
        g_theme = (g_theme + 1) % 6;
      }
      pressed = true;
    }
    if (key == 'b') {         // BtnB 长按 → 亮度循环
      setBrightness((g_brightnessIdx + 1) % BRIGHTNESS_N);
      pressed = true;
    }

    // Restore full power on any button event
    if (pressed) {
      g_lastActivity = millis();
      if (g_powerSave) {
        g_powerSave = false;
        setCpuFrequencyMhz(240);
        g_lastRenderSec = -1;
        g_micFailed = false;
        initAudio();
      }
    }
  }

  // Sample audio for waveform bars (only in normal mode)
  if (!g_powerSave && !g_extremeSave) {
    updateAudio();
  }

  // ── Extreme save: Light Sleep cycling ──
  if (g_extremeSave) {
    // Only render when time actually changes
    bool dirty = (g_s != g_lastRenderSec);
    if (dirty) {
      g_lastRenderSec = g_s;
      render(g_density);
      push();
    }
    // Sleep 200ms (or until BtnA press via GPIO wakeup)
    esp_sleep_enable_timer_wakeup(SLEEP_POLL_US);
    // BtnA = GPIO11 / BtnB = GPIO12 (M5Unified board_M5StickS3 板表)。旧值 GPIO0 是
    // Stick-Bus 侧的 I²C 线, ext0 永远不会触发 → 短按唤不醒。GPIO11/12 都是 RTC GPIO。
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_11, LOW);
    esp_light_sleep_start();
    // Woke up — give hardware time to stabilize, then sample buttons over ~60ms
    // (轻点可能比下一次定时唤醒更早松开, 采样窗口太窄会漏检)
    delay(5);
    bool woke = false;
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
      woke = true; // 按键 GPIO 唤醒 — 直接信任
    } else {
      for (int i = 0; i < 15; i++) {
        M5.update();
        if (M5.BtnA.isPressed() || M5.BtnB.isPressed()) { woke = true; break; }
        delay(4);
      }
    }
    if (woke) {
      g_lastActivity = millis();
      g_extremeSave = false;
      g_powerSave = false;
      setCpuFrequencyMhz(240);
      ArcDisplay::setBrightness(BRIGHTNESS_LEVELS[g_brightnessIdx]);
      pinMode(24, OUTPUT);
      digitalWrite(24, LOW); // green LED back on
      g_lastRenderSec = -1; // force render next frame
      g_micFailed = false; // retry mic on wake
      initAudio();
    }
    return; // skip normal rendering, loop restarts from top
  }

  // ── Normal / power-save rendering ──
  bool shouldRender = true;
  if (g_powerSave) {
    shouldRender = (g_s != g_lastRenderSec);
    if (shouldRender) g_lastRenderSec = g_s;
  }

  if (shouldRender) {
    render(g_density);
    push();
  }

  delay(30);
}

// ── Theme Init ──
static void initThemes() {
  THEMES[0] = { 0x07FF, 0xF81F, 0x5AEF, 0x4A69, 0x03E0, 0x07FF, "Neon Cyan" };
  THEMES[1] = { 0x07E0, 0x03E0, 0x05C0, 0x03E0, 0x01A0, 0x07E0, "Matrix Green" };
  THEMES[2] = { 0x881F, 0x07FF, 0x5A6F, 0x48E8, 0x380E, 0x881F, "Aurora Purple" };
  THEMES[3] = { 0xFD00, 0xF800, 0xAD6B, 0x94A0, 0x9440, 0xFD00, "Ember Orange" };
  THEMES[4] = { 0xF800, 0xFEA0, 0xAD6B, 0x8C40, 0x7000, 0xF800, "Blood Red" };
  THEMES[5] = { 0x6800, 0x9800, 0x4800, 0x3800, 0x1800, 0x7800, "Night Red" };
}

// ── NTP Sync (non-static, shared with wifisetup) ──
bool syncNTP() {
  configTime(g_tzOffset * 3600, 0, "ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org");
  int tries = 0;
  while (tries < NTP_RETRIES) {
    time_t now = time(nullptr);
    if (now > 100000) {  // 1973+ → NTP 已应答
      // Store epoch + base for stable time keeping
      g_epoch = now;
      g_epochBase = millis();
      g_timeValid = true;
      // Update display fields immediately
      struct tm t;
      localtime_r(&now, &t);
      g_h = t.tm_hour;
      g_m = t.tm_min;
      g_s = t.tm_sec;
      g_year = t.tm_year + 1900;
      g_mon = t.tm_mon + 1;
      g_day = t.tm_mday;
      g_wday = t.tm_wday;
      return true;
    }
    delay(NTP_RETRY_MS);
    tries++;
  }
  return false;
}

// ── Timezone NVS ──
static void tzSave(int offset) {
  Preferences p;
  p.begin("cyberclock", false);
  p.putInt("tz", offset);
  p.end();
}
static int tzLoad() {
  Preferences p;
  p.begin("cyberclock", true);
  int v = p.getInt("tz", 999);
  p.end();
  return (v == 999) ? 999 : v;
}

// ── Timezone picker (sprite-buffered) ──
// 注意: 此列表 UI 与 wifisetup.cpp pickSSID() 刻意保持复制
// (两文件各自独立, 遵守"零渲染依赖才拆分"的项目惯例)。
static void pickTimezone() {
  // Same code as stick-cook
  const char* tzNames[] = {
    "UTC-12","UTC-11","UTC-10","UTC-9","UTC-8","UTC-7","UTC-6","UTC-5",
    "UTC-4","UTC-3","UTC-2","UTC-1","UTC+0","UTC+1","UTC+2","UTC+3",
    "UTC+4","UTC+5","UTC+6","UTC+7","UTC+8","UTC+9","UTC+10","UTC+11","UTC+12"
  };
  const int tzVals[] = {-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10,11,12};
  const int N = 25;

  int idx = 12;
  for (int i = 0; i < N; i++) { if (tzVals[i] == g_tzOffset) { idx = i; break; } }

  while (true) {
    M5.update();
    if (M5.BtnA.wasClicked()) { idx = (idx + 1) % N; }
    if (M5.BtnB.wasClicked()) {
      g_tzOffset = tzVals[idx];
      tzSave(g_tzOffset);
      g_tzConfigured = true;
      return;
    }
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setCursor((240 - 16 * 12) / 2, 4);
    canvas.print("Select Timezone");
    canvas.setTextSize(1);
    canvas.setTextColor(c565(120, 120, 120), TFT_BLACK);
    canvas.setCursor((240 - 22 * 6) / 2, 118);
    canvas.print("KEY1:scroll  KEY2:pick");
    int start = idx - 2;
    if (start < 0) start = 0;
    if (start > N - 5) start = N - 5;
    if (start < 0) start = 0;
    int y = 26;
    for (int i = start; i < start + 5 && i < N; i++) {
      bool sel = (i == idx);
      uint16_t bg = sel ? c565(0, 100, 200) : TFT_BLACK;
      canvas.fillRect(4, y, 232, 20, bg);
      canvas.setTextSize(1);
      canvas.setTextColor(sel ? TFT_WHITE : c565(180, 180, 180), bg);
      canvas.setCursor(8, y + 3);
      canvas.print(tzNames[i]);
      y += 22;
    }
    canvas.pushSprite(0, 0);
    delay(30);
  }
}

// ── Time update ──
static void updateTime() {
  if (g_timeValid) {
    // Recompute from epoch + elapsed milliseconds
    uint32_t elapsed_ms = millis() - g_epochBase;
    time_t now_t = g_epoch + elapsed_ms / 1000;
    struct tm* t = localtime(&now_t);
    if (t) {
      g_h = t->tm_hour;
      g_m = t->tm_min;
      g_s = t->tm_sec;
      g_year = t->tm_year + 1900;
      g_mon = t->tm_mon + 1;
      g_day = t->tm_mday;
      g_wday = t->tm_wday;
    }
  } else {
    // Fallback: uptime-based
    uint32_t s = millis() / 1000;
    g_h = (s / 3600) % 24;
    g_m = (s / 60) % 60;
    g_s = s % 60;
  }
}

// ── Matrix rain init (column-drop) ──
static void initMatrixRain() {
  int nch = strlen(RAIN_CHARSETS[(int)g_rainMode]);
  for (int i = 0; i < MATRIX_N; i++) {
    g_rain[i].x = rand() % 240;
    g_rain[i].y = (float)(-(rand() % (TRAIL_LEN * 8 + 20)));
    g_rain[i].spd = 0.4f + (float)(rand() % 100) / 200.0f;
    g_rain[i].headCh = RAIN_CHARSETS[(int)g_rainMode][rand() % nch];
    g_rain[i].headBri = 80 + rand() % 120;
    for (int t = 0; t < TRAIL_LEN; t++)
      g_rain[i].trail[t] = RAIN_CHARSETS[(int)g_rainMode][rand() % nch];
  }
}

// ── Matrix rain update (column-drop) ──
static RainMode nextRainMode(RainMode m) {
  return (RainMode)(((int)m + 1) % (int)RainMode::MODES_N);
}

static void updateMatrixRain() {
  int nch = strlen(RAIN_CHARSETS[(int)g_rainMode]);
  for (int i = 0; i < MATRIX_N; i++) {
    g_rain[i].y += g_rain[i].spd;
    if (g_rain[i].y >= 135 + TRAIL_LEN * 8) {
      g_rain[i].x = rand() % 240;
      g_rain[i].y = -(rand() % (TRAIL_LEN * 12));
      g_rain[i].spd = 0.4f + (float)(rand() % 100) / 200.0f;
      g_rain[i].headCh = RAIN_CHARSETS[(int)g_rainMode][rand() % nch];
      g_rain[i].headBri = 80 + rand() % 120;
    }
    if ((rand() % 100) < 30)
      g_rain[i].headCh = RAIN_CHARSETS[(int)g_rainMode][rand() % nch];
    for (int t = 0; t < TRAIL_LEN; t++) {
      if ((rand() % 100) < 20 + t * 5)
        g_rain[i].trail[t] = RAIN_CHARSETS[(int)g_rainMode][rand() % nch];
    }
  }
}

static void drawMatrixRain() {
  canvas.setTextSize(1);
  for (int i = 0; i < MATRIX_N; i++) {
    int headY = (int)g_rain[i].y;
    int headX = (int)g_rain[i].x;

    // 跳过屏幕外的
    if (headY < -TRAIL_LEN * 8) continue;

    // 画轨迹: 从尾部到头, 越靠近头越亮
    for (int t = TRAIL_LEN - 1; t >= 0; t--) {
      int ty = headY - (t + 1) * 8;
      if (ty < 0) continue;  // 下界交给 canvas 自然裁切 (与头部一致)

      // 亮度: 尾=最暗 (bri/5), 靠近头递增, 头=最亮
      int bri;
      if (t == TRAIL_LEN - 1) {
        bri = 5;             // 最尾: 几乎看不见
      } else {
        bri = g_rain[i].headBri / (TRAIL_LEN - t);
        if (bri < 5) bri = 5;
      }

      uint16_t col = c565(bri/8, bri/2, bri/4);
      canvas.setTextColor(col);
      canvas.setCursor(headX, ty);
      canvas.print(g_rain[i].trail[t]);
    }

    // 画头部 (最亮)
    {
      int bri = g_rain[i].headBri;
      if ((rand() % 100) < 10) bri += 60;
      if (bri > 255) bri = 255;

      // 不同雨效不同色:
      uint16_t headCol;
      switch (g_rainMode) {
        case RainMode::NUMBER: headCol = c565(bri/10, (bri>140)?140:bri, bri/10); break;   // 纯绿
        case RainMode::BINARY: headCol = c565(0, (bri>200)?200:bri, bri/8); break;          // 青
        case RainMode::HEXADECIMAL: headCol = c565(bri/8, bri/2, (bri>100)?100:bri); break;   // 蓝紫
        case RainMode::SYMBOL: headCol = c565((bri>100)?100:bri, bri/2, bri/3); break;     // 暖红
        case RainMode::ALPHA:  headCol = c565(bri/10, bri/3, (bri>140)?140:bri); break;    // 淡蓝
        default:               headCol = c565(bri/6, (bri>120)?120:bri, bri/3); break;     // 蓝绿(Matrix)
      }
      canvas.setTextColor(headCol);
      canvas.setCursor(headX, headY);
      canvas.print(g_rain[i].headCh);
    }
  }
}

// ── Hue → RGB565 for rainbow spectrum ──
static uint16_t hueToRGB565(int hue, uint8_t bri) {
  uint8_t r, g, b;
  int h = hue % 240;
  if (h < 40) {
    r = 255; g = h * 255 / 39; b = 0;
  } else if (h < 80) {
    r = 255 - (h - 40) * 255 / 39; g = 255; b = 0;
  } else if (h < 120) {
    r = 0; g = 255; b = (h - 80) * 255 / 39;
  } else if (h < 160) {
    r = 0; g = 255 - (h - 120) * 255 / 39; b = 255;
  } else if (h < 200) {
    r = (h - 160) * 255 / 39; g = 0; b = 255;
  } else {
    r = 255; g = 0; b = 255 - (h - 200) * 255 / 39;
  }
  r = (uint16_t)r * bri / 255;
  g = (uint16_t)g * bri / 255;
  b = (uint16_t)b * bri / 255;
  return c565(r, g, b);
}

// ── Audio init / stop / update / render ──
// 麦克风连续失败计数: record() 的瞬态失败(缓冲槽占满/锁竞争)不算硬件故障
static int g_micFailStreak = 0;

static void initAudio() {
  if (g_micActive || g_micFailed) return;
  g_micActive = M5.Mic.begin();
  g_audioLastSample = 0;
  g_micFailStreak = 0;
  if (!g_micActive) {
    g_micFailed = true;
    return;
  }
  memset(g_audioBars, 0, AUDIO_BARS);
  memset(g_audioTargets, 0, AUDIO_BARS);
  memset(g_micBuf, 0, sizeof(g_micBuf));
}

static void stopAudio() {
  if (!g_micActive) return;
  M5.Mic.end();
  g_micActive = false;
}

static void updateAudio() {
  if (!g_micActive || g_micFailed) return;
  uint32_t now = millis();
  // Throttle to ~20fps (every 50ms)
  if (now - g_audioLastSample < AUDIO_INTERVAL_MS) return;
  g_audioLastSample = now;

  // Record samples using static buffer (safe for async mic task)
  if (!M5.Mic.record(g_micBuf, AUDIO_SAMPLES)) {
    // 瞬态失败不计故障: M5Unified 的 record() 在"缓冲槽占满/请求锁竞争"时也会立刻返回
    // false (Mic_Class.hpp), 一次卡顿就把波形永久关掉太激进 → 连续 3 次才判硬件故障。
    if (++g_micFailStreak >= 3) {
      stopAudio();
      g_micFailed = true;
      g_micFailStreak = 0;
      memset(g_audioBars, 0, AUDIO_BARS);
    }
    return;
  }
  g_micFailStreak = 0;

  // Compute per-bar amplitude
  int samplesPerBar = AUDIO_SAMPLES / AUDIO_BARS;  // 4 samples per bar
  for (int b = 0; b < AUDIO_BARS; b++) {
    int sum = 0;
    for (int s = 0; s < samplesPerBar; s++) {
      int v = g_micBuf[b * samplesPerBar + s];
      if (v < 0) v = -v;
      sum += v;
    }
    int avg = sum / samplesPerBar;
    // Map 0-32768 to 0-7 (increased peak height by 50%)
    int h = avg * 7 / 6000;
    if (h > 7) h = 7;
    g_audioTargets[b] = (uint8_t)h;
  }

  // Smooth: fast attack, slow decay (VU meter feel)
  for (int i = 0; i < AUDIO_BARS; i++) {
    int t = g_audioTargets[i];
    int cur = g_audioBars[i];
    if (t > cur) {
      cur += 2;           // attack fast
      if (cur > t) cur = t;
    } else if (t < cur) {
      cur--;              // decay slow
      if (cur < 0) cur = 0;
    }
    g_audioBars[i] = (uint8_t)cur;
  }
}

// ── Audio waveform bars ──
static void drawAudioBars() {
  // Baseline floor
  hline(0, 130, 240, c565(5, 5, 12));
  hline(0, 131, 240, c565(3, 3, 8));

  if (!g_micActive || g_micFailed) return;

  for (int i = 0; i < AUDIO_BARS; i++) {
    int h = g_audioBars[i];
    if (h <= 0) continue;

    int x = i * 6;       // 4px bar + 2px gap = 6px per cell
    int hue = i * 240 / AUDIO_BARS;

    // Main bar body (vertical gradient: dim base → bright tip)
    uint16_t bc = hueToRGB565(hue, 80 + h * 25);
    vline(x, 130 - h, h, bc);

    // White flash tip on high peaks
    if (h >= 5) {
      vline(x, 130 - h, 1, (h >= 7) ? TFT_WHITE : hueToRGB565(hue, 255));
    }

    // Bottom glow pixel
    if (h >= 2) {
      vline(x, 129, 1, hueToRGB565(hue, 40));
    }
  }
}

// ── Battery indicator (右上角格子) ──
static int g_batCached = 100;
static uint32_t g_batLastRead = 0;
static void drawBattery() {
  Theme& t = THEMES[g_theme];
  // I2C 读取较慢, 缓存 1s
  uint32_t now = millis();
  if (now - g_batLastRead >= 1000) {
    int bat = M5.Power.getBatteryLevel();
    if (bat < 0) bat = 100;
    g_batCached = bat;
    g_batLastRead = now;
  }
  int bars = g_batCached / 22;
  if (bars > 4) bars = 4;
  for (int i = 0; i < 4; i++) {
    uint16_t bc = (i < bars) ? t.main : c565(15, 15, 15);
    fillRect(210 + i*7, 3, 5, 10, bc);
  }
}

// ── Draw Time ──
static void drawTime() {
  Theme& t = THEMES[g_theme];
  char buf[8];
  snprintf(buf, 8, "%02d:%02d", g_h, g_m);
  int tx = (240 - 5 * 6 * 6) / 2;
  canvas.setTextSize(6);
  canvas.setTextColor(t.accent);
  canvas.setCursor(tx + 1, 42); canvas.print(buf);
  canvas.setCursor(tx - 1, 42); canvas.print(buf);
  canvas.setCursor(tx, 43); canvas.print(buf);
  canvas.setCursor(tx, 41); canvas.print(buf);
  canvas.setTextColor(t.main);
  canvas.setCursor(tx, 42);
  canvas.print(buf);
}

// ── Draw Date ──
static void drawDate() {
  Theme& t = THEMES[g_theme];
  static const char* DAYS[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  char buf[24];
  snprintf(buf, 24, "%04d-%02d-%02d  %s", g_year, g_mon, g_day, DAYS[g_wday % 7]);
  canvas.setTextSize(1);
  canvas.setTextColor(t.date);
  int tw = strlen(buf) * 6;
  canvas.setCursor((240 - tw) / 2, 100);
  canvas.print(buf);
}

// ── Render Modes ──
static void render(Density density) {
  clear(TFT_BLACK);
  updateMatrixRain();
  drawMatrixRain();
  if (density == Density::SAVER) return;   // 数字雨全屏
  drawBattery();
  if (density == Density::TIME) { drawTime(); return; }  // 时钟+数字雨
  drawTime();
  drawDate();
  drawAudioBars();
}

// ── IMU ──
static void updateIMU() {
  static bool imuInited = false;
  if (!imuInited) {
    g_imu.begin();
    g_imu.tau = IMU_TAU;   // config.h 时间常数
    imuInited = true;
  }
  if (!g_imu.available()) return;   // 与 Cardputer 端统一: 用 ArcIMU 自己的判断, 不再问 M5.Imu
  // 省电时降频采样
  static uint32_t lastIMU = 0;
  uint32_t now = millis();
  if (g_powerSave && now - lastIMU < 500) return;
  lastIMU = now;
  g_imu.update();   // pitch/roll 由 update() 写入, 无需再 getAttitude() 自赋值
}

// ── Shake Detection ──
static void checkShake() {
  if (!g_imu.available()) return;
  // 省电模式下不响应摇动 (IMU 已降频, 阈值行为不一致)
  if (g_powerSave || g_extremeSave) return;
  float dp = fabsf(g_imu.pitch - g_prevP);
  float dr = fabsf(g_imu.roll - g_prevR);
  g_prevP = g_imu.pitch;
  g_prevR = g_imu.roll;
  // 单轴主导的晃动也能触发 (原版 && 要求双轴同时变化, 常漏检)
  if (fmaxf(dp, dr) > SHAKE_THRESHOLD) {
    g_shakeCnt++;
    if (g_shakeCnt >= SHAKE_COUNT_TRIGGER) {
      g_theme = (g_theme + 1) % 6;
      g_shakeCnt = 0;
    }
  } else {
    if (g_shakeCnt > 0) g_shakeCnt--;
  }
}

// ── Brightness ──
static void setBrightness(int idx) {
  if (idx < 0) idx = 0;
  if (idx >= BRIGHTNESS_N) idx = BRIGHTNESS_N - 1;
  g_brightnessIdx = idx;
  ArcDisplay::setBrightness(BRIGHTNESS_LEVELS[idx]);
}
