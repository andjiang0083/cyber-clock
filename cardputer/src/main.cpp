// ────────────────────────────────────────────────────────────
// CYBER CLOCK — M5Cardputer 赛博朋克桌面时钟
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
  "0123456789",
  "01",
  "0123456789ABCDEF",
  "#*+:.<>/|~@%&$",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
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

// ── Audio state (Cardputer有内置mic) ──
static bool g_micActive = false;
static bool g_micFailed = false;
static uint8_t g_audioBars[AUDIO_BARS] = {0};
static uint8_t g_audioTargets[AUDIO_BARS] = {0};
static uint32_t g_audioLastSample = 0;
static int16_t g_micBuf[AUDIO_SAMPLES];

// ── IMU (Cardputer ADV 有 BMI270) ──
static ArcIMU g_imu;
static float g_prevP = 0, g_prevR = 0;
static int g_shakeCnt = 0;

// ── Brightness ──
static int g_brightnessIdx = BRIGHTNESS_DEFAULT_IDX;

// ── Power save ──
static uint32_t g_lastActivity = 0;
static bool g_powerSave = false;
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
static void render(Density density);
static void setBrightness(int idx);
static void initAudio();
static void stopAudio();
static void updateAudio();
static void drawAudioBars();
static void updateIMU();
static void checkShake();
static void pickTimezone();
static int tzLoad();
static RainMode nextRainMode(RainMode m);

// ── USB 供电判定 (缓存 1s) ──
// 不要用 M5.Power.isCharging(): 那是充电器的 STAT 脚, 充满后立刻变 false。
// Cardputer 没有 PMIC → getVBUSVoltage() 返回 -1, 这里恒为 false (与旧行为一致, 无回归);
// 有 M5PM1 的 StickS3 才返回真实 VBUS 电压(mV)。
static bool g_usbPlugged = false;
static uint32_t g_usbLastRead = 0;
static bool usbPowerPresent() {
  uint32_t now = millis();
  if (now - g_usbLastRead >= 1000) {
    g_usbLastRead = now;
    int16_t mv = M5.Power.getVBUSVoltage();
    g_usbPlugged = (mv > 1000);
  }
  return g_usbPlugged;
}

// ── Setup ──
void setup() {
  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::board_M5Cardputer;
  M5Cardputer.begin(cfg);

  // 随机种子: 数字雨每次开机序列不同
  srand(esp_random());

  // Landscape 240×135
  ArcDisplay::begin(SCREEN_ROTATION, BRIGHTNESS_LEVELS[g_brightnessIdx]);

  // sprite 分配失败 → canvas 的绘制会静默失效: 整屏黑、不崩溃、日志空白
  // (docs/bugfix-black-screen-spec.md)。显式熔断, 把黑屏变成可读信息。
  if (!ArcDisplay::canvas.getBuffer()) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(12, 58);
    M5.Display.print("SPRITE FAIL");
    while (true) delay(1000);
  }

  initThemes();
  initMatrixRain();

  // ── Custom splash with matrix rain background ──
  {
    uint32_t splashStart = millis();
    while (millis() - splashStart < 3000) {
      updateMatrixRain();
      canvas.startWrite();
      clear(TFT_BLACK);
      drawMatrixRain();

      canvas.fillRect(0, 20, 240, 55, c565(0, 0, 0));

      canvas.setTextSize(2);
      canvas.setTextColor(c565(80, 255, 120), c565(0, 0, 0));
      int tw = strlen("CYBER CLOCK") * 12;
      canvas.setCursor((240 - tw) / 2, 24);
      canvas.print("CYBER CLOCK");

      canvas.setTextSize(1);
      canvas.setTextColor(c565(150, 200, 160), c565(0, 0, 0));
      tw = strlen(VERSION) * 6;
      canvas.setCursor((240 - tw) / 2, 50);
      canvas.print(VERSION);

      canvas.setTextSize(1);
      canvas.setTextColor(c565(140, 200, 160), TFT_BLACK);
      canvas.setCursor(4, 110);
      canvas.print("[S]=Screen");
      canvas.setCursor(80, 110);
      canvas.print("[C]=Theme");
      canvas.setCursor(152, 110);
      canvas.print("[W]=WiFi");
      canvas.setCursor(4, 122);
      canvas.print("[R]=RainFX");
      canvas.setCursor(132, 122);
      canvas.print("[+/-]=Bright");

      canvas.endWrite();
      push();

      // 可跳过: 任意按键
      M5Cardputer.update();
      auto ks = M5Cardputer.Keyboard.keysState();
      if (ks.word.size() > 0 || ks.enter || ks.del) break;
      if (ks.space || ks.tab) break;
      delay(10);
    }
    ArcDisplay::clear();
    ArcDisplay::push();
  }

  // Load timezone from NVS
  int savedTz = tzLoad();
  if (savedTz != 999) { g_tzOffset = savedTz; g_tzConfigured = true; }

  // WiFi setup — 成功/失败/取消三条路径都要关射频并复位空闲计时
  {
    bool wifiOk = wifiSetupBegin(false);
    if (wifiOk && !g_tzConfigured) {
      pickTimezone();
      syncNTP();     // 需在关射频之前完成
    }
    wifiStopAll();
    g_lastActivity = millis();
  }

  // IMU init (silent fallback if no sensor)
  g_imu.begin();

  // Audio init
  initAudio();
}

// ── Keyboard edge detection ──
static uint32_t g_prevHeldMask = 0;

// ── 获取按键 (符号键直接返回, 字母键做边沿检测) ──
static int getKey() {
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange()) return 0;

  auto ks = M5Cardputer.Keyboard.keysState();
  if (ks.word.size() == 0) { g_prevHeldMask = 0; return 0; }

  // 符号键: 直接返回 (不经过边沿检测)
  for (auto key : ks.word) {
    if (key == '+' || key == '=') return '+';
    if (key == '-' || key == '_') return '-';
    if (key == '.' || key == '>') return '.';
    if (key == ';' || key == ':') return ';';
  }

  // 字母键: 边沿检测 (按住不重复)
  uint32_t mask = 0;
  for (auto key : ks.word) {
    char k = key;
    if (k >= 'A' && k <= 'Z') k += 32;
    if (k >= 'a' && k <= 'z') mask |= (1UL << (k - 'a'));
  }
  uint32_t edge = mask & ~g_prevHeldMask;
  g_prevHeldMask = mask;

  for (auto key : ks.word) {
    char k = key;
    if (k >= 'A' && k <= 'Z') k += 32;
    if (k >= 'a' && k <= 'z') {
      uint32_t bit = 1UL << (k - 'a');
      if (bit & edge) return key;
    }
  }

  return 0;   // Enter 分支已删: ks.enter 时 ks.word 必为空, 上面早退已拦住 (不可达代码)
}

// ── Loop ──
void loop() {
  uint32_t now = millis();

  // ── Power save ──
  // 插着 USB 就跳过省电; 不再要求 g_timeValid (没对时的设备也要省电)
  bool plugged = usbPowerPresent();
  if (!g_powerSave && now - g_lastActivity > IDLE_POWERSAVE_MS && !plugged) {
    g_powerSave = true;
    setCpuFrequencyMhz(80);
  }
  // 省电中插上 USB → 立刻恢复正常刷新
  if (plugged && g_powerSave) {
    g_powerSave = false;
    setCpuFrequencyMhz(240);
    g_lastRenderSec = -1;
  }

  // ── Input via keyboard (edge-detected) ──
  {
    int key = getKey();
    bool pressed = false;

    if (key == 's' || key == 'S') {   // [S] → 界面循环
      g_density = (Density)(((int)g_density + 1) % 3);
      pressed = true;
    }
    if (key == 'r' || key == 'R') {   // [R] → 切换数字雨效果
      g_rainMode = nextRainMode(g_rainMode);
      pressed = true;
    }
    if (key == 'c' || key == 'C') {   // [C] → 按界面切换
      if (g_density == Density::SAVER) {
        g_rainMode = nextRainMode(g_rainMode);
      } else {
        g_theme = (g_theme + 1) % 6;
      }
      pressed = true;
    }
    if (key == 'w' || key == 'W') {   // [W] → WiFi连接 & 对时
      ArcDisplay::clear();
      ArcDisplay::push();
      delay(100);
      wifiRunSetupFlow();
      wifiStopAll();
      g_lastActivity = millis();
      pressed = true;
    }
    if (key == '+' || key == '=') {   // [+] → 亮度+
      setBrightness((g_brightnessIdx + 1) % BRIGHTNESS_N);
      pressed = true;
    }
    if (key == '-' || key == '_') {   // [-] → 亮度-
      setBrightness((g_brightnessIdx - 1 + BRIGHTNESS_N) % BRIGHTNESS_N);
      pressed = true;
    }

    // Restore full power on any key
    if (pressed) {
      g_lastActivity = millis();
      if (g_powerSave) {
        g_powerSave = false;
        setCpuFrequencyMhz(240);
        g_lastRenderSec = -1;
      }
    }
  }

  updateTime();
  updateIMU();
  checkShake();

  // Audio sample
  if (!g_powerSave) {
    updateAudio();
  }

  // ── Power-save rendering (1 fps) ──
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

// ── NTP Sync ──
bool syncNTP() {
  configTime(g_tzOffset * 3600, 0, "ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org");
  int tries = 0;
  while (tries < NTP_RETRIES) {
    time_t now = time(nullptr);
    if (now > 100000) {  // 1973+ → NTP 已应答
      g_epoch = now;
      g_epochBase = millis();
      g_timeValid = true;
      struct tm t;
      localtime_r(&now, &t);
      g_h = t.tm_hour; g_m = t.tm_min; g_s = t.tm_sec;
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

// ── Timezone picker (keyboard) ──
static void pickTimezone() {
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
    M5Cardputer.update();
    auto ks = M5Cardputer.Keyboard.keysState();
    for (auto k : ks.word) {
      // 与 WiFi 列表(pickSSID)统一: ';' = 上, '.' = 下 (旧代码两处相反, 用户会按错方向)
      if (k == ';' || k == ':') idx = (idx - 1 + N) % N; // 上
      if (k == '.' || k == '>') idx = (idx + 1) % N;     // 下
      if (k == 'n' || k == 'N') {                       // 确认(N键)
        g_tzOffset = tzVals[idx];
        tzSave(g_tzOffset);
        g_tzConfigured = true;
        return;
      }
    }
    if (ks.enter) {                                     // 确认(Enter)
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
    canvas.setCursor((240 - 29 * 6) / 2, 118);
    canvas.print("[;]=Up  [.]=Down  [N]=Confirm");
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
    uint32_t elapsed_ms = millis() - g_epochBase;
    time_t now_t = g_epoch + elapsed_ms / 1000;
    struct tm* t = localtime(&now_t);
    if (t) {
      g_h = t->tm_hour; g_m = t->tm_min; g_s = t->tm_sec;
      g_year = t->tm_year + 1900;
      g_mon = t->tm_mon + 1;
      g_day = t->tm_mday;
      g_wday = t->tm_wday;
    }
  } else {
    uint32_t s = millis() / 1000;
    g_h = (s / 3600) % 24;
    g_m = (s / 60) % 60;
    g_s = s % 60;
  }
}

// ── Matrix rain init (column-drop) ──
static RainMode nextRainMode(RainMode m) {
  return (RainMode)(((int)m + 1) % (int)RainMode::MODES_N);
}

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

// ── Matrix rain update ──
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

// ── Matrix rain draw (column-drop) ──
static void drawMatrixRain() {
  canvas.setTextSize(1);
  for (int i = 0; i < MATRIX_N; i++) {
    int headY = (int)g_rain[i].y;
    int headX = (int)g_rain[i].x;
    if (headY < -TRAIL_LEN * 8) continue;

    for (int t = TRAIL_LEN - 1; t >= 0; t--) {
      int ty = headY - (t + 1) * 8;
      if (ty < 0) continue;  // 下界交给 canvas 自然裁切 (与头部一致)
      int bri;
      if (t == TRAIL_LEN - 1) {
        bri = 5;
      } else {
        bri = g_rain[i].headBri / (TRAIL_LEN - t);
        if (bri < 5) bri = 5;
      }
      uint16_t col = c565(bri/8, bri/2, bri/4);
      canvas.setTextColor(col);
      canvas.setCursor(headX, ty);
      canvas.print(g_rain[i].trail[t]);
    }

    {
      int bri = g_rain[i].headBri;
      if ((rand() % 100) < 10) bri += 60;
      if (bri > 255) bri = 255;

      uint16_t headCol;
      switch (g_rainMode) {
        case RainMode::NUMBER:      headCol = c565(bri/10, (bri>140)?140:bri, bri/10); break;
        case RainMode::BINARY:      headCol = c565(0, (bri>200)?200:bri, bri/8); break;
        case RainMode::HEXADECIMAL: headCol = c565(bri/8, bri/2, (bri>100)?100:bri); break;
        case RainMode::SYMBOL:      headCol = c565((bri>100)?100:bri, bri/2, bri/3); break;
        case RainMode::ALPHA:       headCol = c565(bri/10, bri/3, (bri>140)?140:bri); break;
        default:                    headCol = c565(bri/6, (bri>120)?120:bri, bri/3); break;
      }
      canvas.setTextColor(headCol);
      canvas.setCursor(headX, headY);
      canvas.print(g_rain[i].headCh);
    }
  }
}

// ── Battery indicator ──
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

// ── Audio init / stop / update ──
// 麦克风连续失败计数: record() 的瞬态失败(缓冲槽占满/锁竞争)不算硬件故障
static int g_micFailStreak = 0;

static void initAudio() {
  if (g_micActive || g_micFailed) return;
  g_micActive = M5.Mic.begin();
  g_audioLastSample = 0;
  g_micFailStreak = 0;
  if (!g_micActive) { g_micFailed = true; return; }
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
  if (now - g_audioLastSample < AUDIO_INTERVAL_MS) return;
  g_audioLastSample = now;
  if (!M5.Mic.record(g_micBuf, AUDIO_SAMPLES)) {
    // 瞬态失败不计故障 (Mic_Class.hpp: 缓冲槽占满/锁竞争时 record() 也会 false),
    // 连续 3 次才判硬件故障 — 否则一次卡顿波形就永久消失。
    if (++g_micFailStreak >= 3) {
      stopAudio();
      g_micFailed = true;
      g_micFailStreak = 0;
      memset(g_audioBars, 0, AUDIO_BARS);
    }
    return;
  }
  g_micFailStreak = 0;
  int samplesPerBar = AUDIO_SAMPLES / AUDIO_BARS;
  for (int b = 0; b < AUDIO_BARS; b++) {
    int sum = 0;
    for (int s = 0; s < samplesPerBar; s++) {
      int v = g_micBuf[b * samplesPerBar + s];
      if (v < 0) v = -v;
      sum += v;
    }
    int avg = sum / samplesPerBar;
    int h = avg * 7 / 6000;
    if (h > 7) h = 7;
    g_audioTargets[b] = (uint8_t)h;
  }
  // Smooth: fast attack, slow decay (VU meter feel)
  for (int i = 0; i < AUDIO_BARS; i++) {
    int t = g_audioTargets[i];
    int cur = g_audioBars[i];
    if (t > cur) {
      cur += 2;
      if (cur > t) cur = t;
    } else if (t < cur) {
      cur--;
      if (cur < 0) cur = 0;
    }
    g_audioBars[i] = (uint8_t)cur;
  }
}

// ── Audio waveform bars ──
static void drawAudioBars() {
  hline(0, 130, 240, c565(5, 5, 12));
  hline(0, 131, 240, c565(3, 3, 8));
  if (!g_micActive || g_micFailed) return;
  for (int i = 0; i < AUDIO_BARS; i++) {
    int h = g_audioBars[i];
    if (h <= 0) continue;
    int x = i * 6;
    int hue = i * 240 / AUDIO_BARS;
    uint16_t bc = hueToRGB565(hue, 80 + h * 25);
    vline(x, 130 - h, h, bc);
    if (h >= 5) {
      vline(x, 130 - h, 1, (h >= 7) ? TFT_WHITE : hueToRGB565(hue, 255));
    }
    if (h >= 2) {
      vline(x, 129, 1, hueToRGB565(hue, 40));
    }
  }
}

// ── IMU ──
static void updateIMU() {
  // setup() 里已经 g_imu.begin(); 这里不再重复 begin (旧代码会 begin 两次)
  if (!g_imu.available()) return;
  static uint32_t lastIMU = 0;
  uint32_t now = millis();
  if (g_powerSave && now - lastIMU < 500) return;
  lastIMU = now;
  g_imu.update();
}

// ── Shake Detection ──
static void checkShake() {
  if (!g_imu.available()) return;
  // 省电模式下不响应摇动 (IMU 已降频, 阈值行为不一致)
  if (g_powerSave) return;
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

// ── Brightness ──
static void setBrightness(int idx) {
  if (idx < 0) idx = 0;
  if (idx >= BRIGHTNESS_N) idx = BRIGHTNESS_N - 1;
  g_brightnessIdx = idx;
  ArcDisplay::setBrightness(BRIGHTNESS_LEVELS[idx]);
}
