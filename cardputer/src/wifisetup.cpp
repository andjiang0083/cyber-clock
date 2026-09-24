// ────────────────────────────────────────────────────────────
// wifisetup.cpp — WiFi configuration flow for cyber-clock
// ────────────────────────────────────────────────────────────
#include "wifisetup.h"
#include "textinput.h"
#include "timesync.h"
#include "idf/idf_wifi.h"
#include <M5Cardputer.h>

static Preferences g_prefs;
static bool g_timeSynced = false;
bool wifiLoadCredentials(String& ssid, String& pass) {
  g_prefs.begin("cyberclock", true);
  ssid = g_prefs.getString("wifi_ssid", "");
  pass = g_prefs.getString("wifi_pass", "");
  g_prefs.end();
  return ssid.length() > 0;
}

void wifiSaveCredentials(const String& ssid, const String& pass) {
  g_prefs.begin("cyberclock", false);
  g_prefs.putString("wifi_ssid", ssid);
  g_prefs.putString("wifi_pass", pass);
  g_prefs.end();
}

void wifiClearCredentials() {
  g_prefs.begin("cyberclock", false);
  g_prefs.remove("wifi_ssid");
  g_prefs.remove("wifi_pass");
  g_prefs.end();
}

// ── Display helpers ──
static void drawMsg(const char* line1, const char* line2 = nullptr) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  int w1 = strlen(line1) * 12;
  M5.Display.setCursor((240 - w1) / 2, 45);
  M5.Display.print(line1);
  if (line2) {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(180, 180, 180), TFT_BLACK);
    int w2 = strlen(line2) * 6;
    if (w2 > 220) { w2 = 220; }
    M5.Display.setCursor((240 - w2) / 2, 75);
    M5.Display.print(line2);
  }
}

// ── Draw a scrollable list of SSIDs (Cardputer 键盘版) ──
static int pickSSID(const std::vector<LauncherWifiAp>& aps) {
  if (aps.empty()) return -1;
  int idx = 0;
  bool redraw = true;

  while (true) {
    M5Cardputer.update();

    // 键盘输入
    if (M5Cardputer.Keyboard.isChange()) {
      auto ks = M5Cardputer.Keyboard.keysState();
      for (auto key : ks.word) {
        if (key == ';' || key == ':') { // 上
          idx = (idx - 1 + aps.size()) % aps.size();
          redraw = true;
        }
        if (key == '.' || key == '>') { // 下
          idx = (idx + 1) % aps.size();
          redraw = true;
        }
      }
      if (ks.enter) { return idx; }    // Enter 确认
      if (ks.del) { return -1; }       // Del 取消
    }

    if (!redraw) { delay(30); continue; }
    redraw = false;

    M5.Display.fillScreen(TFT_BLACK);

    // Header
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor((240 - 11 * 12) / 2, 4);
    M5.Display.print("Select WiFi");

    // Instruction
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(120, 120, 120), TFT_BLACK);
    M5.Display.setCursor((240 - 26 * 6) / 2, 118);
    M5.Display.print("[;]=Up  [.]=Down  Enter=OK");

    // Draw list of visible SSIDs (max 4 at a time)
    int start = idx - 2;
    if (start < 0) start = 0;
    int end = start + 4;
    if (end > (int)aps.size()) end = aps.size();
    if (end - start < 4 && start > 0) start = end - 4;
    if (start < 0) start = 0;

    int y = 26;
    for (int i = start; i < end; i++) {
      bool sel = (i == idx);
      uint16_t bg = sel ? M5.Display.color565(0, 100, 200) : TFT_BLACK;
      M5.Display.fillRect(4, y, 232, 20, bg);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(sel ? TFT_WHITE : M5.Display.color565(180, 180, 180), bg);
      M5.Display.setCursor(8, y + 3);

      String ssid = aps[i].ssid.c_str();
      if (ssid.length() > 22) ssid = ssid.substring(0, 20) + "..";
      M5.Display.print(ssid.c_str());

      int rssi = aps[i].rssi;
      const char* sig = (rssi > -50) ? "!!!" : (rssi > -70) ? "!!" : (rssi > -80) ? "!" : "~";
      M5.Display.setCursor(215, y + 3);
      M5.Display.print(sig);

      y += 22;
    }
    delay(30);
  }
}

// ── Main WiFi setup flow ──
bool wifiRunSetupFlow() {
  drawMsg("Scanning WiFi...");
  delay(500);

  // Scan
  std::vector<LauncherWifiAp> aps;
  int n = launcherWifiScan(aps);
  if (n <= 0) {
    drawMsg("No networks found", "Try again later");
    delay(2000);
    return false;
  }

  // Pick SSID
  int sel = pickSSID(aps);
  if (sel < 0) return false;

  String ssid = aps[sel].ssid.c_str();

  // Enter password
  String pass;
  char prompt[32];
  snprintf(prompt, 32, "Password for %s", ssid.c_str());
  if (!textInput(prompt, pass, 64)) {
    return false;
  }

  // Connect
  drawMsg("Connecting...", ssid.c_str());
  delay(300);

  if (launcherWifiConnect(ssid.c_str(), pass.c_str(), 15000)) {
    wifiSaveCredentials(ssid, pass);
    drawMsg("WiFi connected!", ssid.c_str());
    delay(1000);

    // NTP sync
    if (syncNTP()) {
      g_timeSynced = true;
      drawMsg("Time synced!");
    } else {
      drawMsg("NTP failed", "Time may be wrong");
    }
    delay(1000);
    return true;
  } else {
    // Check if wrong password
    auto status = launcherWifiConnectStatus(ssid.c_str(), pass.c_str(), 5000);
    if (status == LauncherWifiConnectState::WrongPassword) {
      drawMsg("Wrong password!", "Try again");
    } else {
      drawMsg("Connection failed", ssid.c_str());
    }
    launcherWifiStop();
    delay(2000);
    return false;
  }
}

// ── WiFi begin ──
bool wifiSetupBegin(bool forceReconfig) {
  // If user held KEY1 at boot, skip saved creds
  if (forceReconfig) {
    return wifiRunSetupFlow();
  }

  // Try loading saved credentials first
  String ssid, pass;
  if (wifiLoadCredentials(ssid, pass) && ssid.length() > 0) {
    drawMsg("Connecting to", ssid.c_str());
    if (launcherWifiConnect(ssid.c_str(), pass.c_str(), 10000)) {
      // NTP sync
      for (int i = 0; i < 5 && !g_timeSynced; i++) {
        if (syncNTP()) {
          g_timeSynced = true;
          drawMsg("Time synced!");
          delay(500);
          return true;
        }
        delay(300);
      }
      drawMsg("NTP timeout", "Will retry later");
      delay(1000);
      return true; // connected even without NTP
    }
    drawMsg("Saved WiFi failed", "Enter setup mode");
    delay(1500);
    // Fall through to setup flow
  }

  // No valid credentials → run setup
  return wifiRunSetupFlow();
}

bool wifiTimeSynced() { return g_timeSynced; }

// ── Unified WiFi shutdown (esp_wifi layer) ──
// 与旧的 WiFi.disconnect(true)+WiFi.mode(WIFI_OFF) 不同, 这里走 idf_wifi
// 自己的状态机, 避免 Arduino WiFiGeneric 与裸 esp_wifi 双栈状态脱节。
void wifiStopAll() {
  launcherWifiStop();
}
