// ────────────────────────────────────────────────────────────
// wifisetup.cpp — WiFi configuration flow for cyber-clock
// v1.1.2: big-text carousel picker + AP/QR phone-provisioning
// fallback (HOLD KEY1 on the SSID screen). Ported from Stick
// Clock v0.2.2; NVS stays "cyberclock", AP IP 192.168.4.1 is
// fixed by idf_wifi launcherWifiStartAp(). Direct-to-display.
// ────────────────────────────────────────────────────────────
#include "wifisetup.h"
#include "textinput.h"
#include "timesync.h"
#include "idf/idf_wifi.h"
#include <M5Unified.h>

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

// ── Display helpers (direct, size-clamped) ──
static void drawMsg(const char* line1, const char* line2 = nullptr) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  int w1 = strlen(line1) * 12;
  M5.Display.setCursor((240 - w1) / 2, 45);
  M5.Display.print(line1);
  if (line2) {
    char buf[40];
    strncpy(buf, line2, 38); buf[38] = '\0';   // fit: 39 chars max @ size=1
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(180, 180, 180), TFT_BLACK);
    M5.Display.setCursor((240 - (int)strlen(buf) * 6) / 2, 78);
    M5.Display.print(buf);
  }
}

#if defined(ARDUINO_ARCH_ESP32)
#include <WebServer.h>
#include <DNSServer.h>
#include "qrcode.h"

static WebServer g_apServer(80);
static DNSServer g_dnsServer;
static volatile bool g_apSaved = false;   // set by POST /wifi

// ── SSID carousel picker (ported from stick-cook v0.2.2) ──
// KEY1 tap = next (wraps) · HOLD KEY1 ≥800ms = QR phone setup
// KEY2 tap = pick · KEY2 hold = cancel.
// Returns index, -1 on cancel, -2 when QR mode requested.
static int pickSSID(const std::vector<LauncherWifiAp>& aps) {
  if (aps.empty()) return -1;
  int idx = 0;
  bool redraw = true;

  while (true) {
    M5.update();
    if (M5.BtnA.wasClicked()) {
      idx = (idx + 1) % aps.size();
      redraw = true;
    } else if (M5.BtnA.pressedFor(800)) {
      return -2;                       // QR fallback requested
    }
    if (M5.BtnB.wasClicked()) { return idx; }
    if (M5.BtnB.pressedFor(600))  { return -1; }

    if (!redraw) { delay(30); continue; }
    redraw = false;

    auto& d = M5.Display;
    d.fillScreen(TFT_BLACK);

    // prev / next neighbours, faint size=1 (carousel affordance)
    const int n = aps.size();
    char nb[40];
    auto neighbour = [&](int i, char* out) {
      String s = aps[i].ssid.c_str();
      if ((int)s.length() > 36) s = s.substring(0, 34) + "..";
      strncpy(out, s.c_str(), 38); out[38] = '\0';
    };

    d.setTextSize(1);
    neighbour((idx + n - 1) % n, nb);
    d.setTextColor(M5.Display.color565(70, 78, 90));
    d.setCursor((240 - (int)strlen(nb) * 6) / 2, 8);
    d.print(nb);

    neighbour((idx + 1) % n, nb);
    d.setCursor((240 - (int)strlen(nb) * 6) / 2, 86);
    d.print(nb);

    // current SSID — big size=3, max 12 chars
    String cur = aps[idx].ssid.c_str();
    if ((int)cur.length() > 12) cur = cur.substring(0, 10) + "..";
    d.setTextSize(3);
    d.setTextColor(TFT_WHITE);
    int cw = (int)cur.length() * 18;
    d.setCursor((240 - cw) / 2, 34);
    d.print(cur.c_str());

    // RSSI bars + dBm, centered as one group
    int rssi = aps[idx].rssi;
    int lit = (rssi > -50) ? 4 : (rssi > -62) ? 3 : (rssi > -74) ? 2 : 1;
    char db[10];
    snprintf(db, sizeof(db), "%ddBm", rssi);
    int barsW = 3 * 7 + 5;                 // 4 bars, step 7
    int gw = barsW + 8 + (int)strlen(db) * 6;
    int gx = (240 - gw) / 2;
    for (int i = 0; i < 4; i++) {
      int hgt = 5 + i * 4;
      uint16_t c = (i < lit) ? M5.Display.color565(0, 220, 120)
                              : M5.Display.color565(40, 50, 48);
      d.fillRect(gx + i * 7, 76 - hgt, 5, hgt, c);
    }
    d.setTextSize(1);
    d.setTextColor(lit >= 3 ? M5.Display.color565(0, 220, 120)
                            : M5.Display.color565(220, 190, 80));
    d.setCursor(gx + barsW + 8, 68);
    d.print(db);

    // hints
    d.setTextSize(1);
    d.setTextColor(M5.Display.color565(140, 140, 150));
    d.setCursor((240 - 21 * 6) / 2, 104);
    d.print("KEY1:next   KEY2:pick");
    d.setTextColor(M5.Display.color565(80, 90, 100));
    d.setCursor((240 - 26 * 6) / 2, 118);
    d.print("HOLD A: QR setup on phone");

    // position counter, top-right
    char pc[12];
    snprintf(pc, sizeof(pc), "%d/%d", idx + 1, n);
    d.setTextColor(M5.Display.color565(80, 90, 100));
    d.setCursor(236 - (int)strlen(pc) * 6, 2);
    d.print(pc);

    delay(30);
  }
}

// ── AP + QR provisioning (phone enters WiFi credentials) ──
static void buildApName(char* out, size_t cap) {
  std::string mac = launcherWifiMac();       // "AA:BB:CC:DD:EE:FF"
  if (mac.size() >= 17) {
    snprintf(out, cap, "CyberClock-%c%c%c%c", mac[12], mac[13], mac[15], mac[16]);
  } else {
    snprintf(out, cap, "CyberClock");
  }
}

static void handleApRoot() {
  static const char* HTML = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Cyber Clock Setup</title><style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0a1418;color:#ade;font-family:-apple-system,sans-serif;text-align:center;padding:20px 16px}
h1{color:#4cf;font-size:20px;margin-bottom:4px}
p{color:#68a;font-size:13px;margin-bottom:14px}
.box{background:#0f1a20;border:1px solid #1a3450;border-radius:10px;padding:14px;text-align:left}
label{color:#6ac;font-size:12px;display:block;margin:6px 0 3px}
input{width:100%;padding:8px 10px;font-size:14px;border:1px solid #2a5070;border-radius:6px;background:#0d1820;color:#cef;outline:none}
input:focus{border-color:#4cf}
button{background:#1a5080;color:#eef;border:none;padding:12px 28px;font-size:15px;border-radius:8px;cursor:pointer;width:100%;font-weight:600;margin-top:10px}
button:active{background:#145a90}
.ok{color:#4cf;font-size:13px;margin-top:8px;text-align:center}
</style></head><body>
<h1>Cyber Clock</h1>
<p>Enter your WiFi name &amp; password,<br>then tap <strong>Save</strong>.</p>
<div class="box">
<label>WiFi Name (SSID)</label><input type="text" id="ssid" placeholder="e.g. MyHomeWiFi">
<label>Password (leave blank if open)</label><input type="password" id="pass" placeholder="your WiFi password">
<div style="color:#68a;font-size:12px;margin-top:8px" id="tz"></div>
<button onclick="saveWifi()" id="btn">Save &amp; Connect</button>
<div id="st"></div></div>
<script>
function saveWifi(){
 var s=document.getElementById('ssid').value.trim();
 if(!s){alert('Enter WiFi name');return}
 var p=document.getElementById('pass').value.trim();
 var tz=-new Date().getTimezoneOffset();
 var b=document.getElementById('btn'),st=document.getElementById('st');
 b.disabled=true;st.textContent='Saving...';
 fetch('/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
 body:'ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)+'&tz='+tz})
 .then(function(r){return r.text()}).then(function(m){st.textContent=m;b.textContent='Done';st.className='ok'})
 .catch(function(e){st.textContent='X '+e;b.disabled=false})}
var tz=-new Date().getTimezoneOffset(),tn=Intl.DateTimeFormat().resolvedOptions().timeZone||'';
document.getElementById('tz').textContent='Timezone: UTC'+(tz>=0?'+':'')+(tz/60)+' '+tn;
</script></body></html>)rawliteral";
  g_apServer.send(200, "text/html", HTML);
}

static void handleApPost() {
  if (!g_apServer.hasArg("ssid")) { g_apServer.send(400, "text/plain", "Missing SSID"); return; }
  String ssid = g_apServer.arg("ssid"); ssid.trim();
  if (ssid.length() == 0) { g_apServer.send(400, "text/plain", "Empty SSID"); return; }
  String pass = g_apServer.arg("pass"); pass.trim();
  wifiSaveCredentials(ssid, pass);
  if (g_apServer.hasArg("tz")) {                 // browser timezone → cyberclock/tz
    int tzH = g_apServer.arg("tz").toInt() / 60;
    if (tzH >= -12 && tzH <= 14) {
      g_tzOffset = tzH;
      Preferences p; p.begin("cyberclock", false); p.putInt("tz", tzH); p.end();
    }
  }
  g_apServer.send(200, "text/plain", "Saved! Connecting...");
  g_apSaved = true;
}

static void handleApNotFound() {
  g_apServer.sendHeader("Location", "/", true);
  g_apServer.send(302, "text/plain", "");
}

static void drawQrScreen(const char* apName) {
  QRCode qr;
  uint8_t qrData[qrcode_getBufferSize(2)];      // version 2, fits WIFI:T:nopass;S:...;;
  bool qrOk = qrcode_initText(&qr, qrData, 2, ECC_LOW,
                              (String("WIFI:T:nopass;S:") + apName + ";;").c_str()) == 0;

  M5.Display.fillScreen(TFT_BLACK);
  auto& d = M5.Display;
  int mod = 3;
  if (qrOk) {
    int qrPx = qr.size * mod;                 // 75 px @ v2
    int qx = (240 - qrPx) / 2, qy = 16;
    for (int y = 0; y < qr.size; y++)
      for (int x = 0; x < qr.size; x++)
        if (qrcode_getModule(&qr, x, y))
          d.fillRect(qx + x * mod, qy + y * mod, mod, mod, TFT_WHITE);
  } else {
    drawMsg("QR gen failed", NULL);
  }
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE);
  char l[40];
  snprintf(l, sizeof(l), "AP: %s", apName);
  d.setCursor((240 - (int)strlen(l) * 6) / 2, qrOk ? 94 : 108);
  d.print(l);
  if (qrOk) {
    const char* s = "type WiFi + password on phone";
    d.setTextColor(M5.Display.color565(140, 170, 190));
    d.setCursor((240 - (int)strlen(s) * 6) / 2, 106);
    d.print(s);
  }
  const char* bk = "KEY2: back";
  d.setTextColor(M5.Display.color565(80, 90, 100));
  d.setCursor((240 - (int)strlen(bk) * 6) / 2, qrOk ? 120 : 94);
  d.print(bk);

  // serve until saved / KEY2 / 5-min timeout
  g_apSaved = false;
  uint32_t start = millis();
  while (!g_apSaved && millis() - start < 300000) {
    g_dnsServer.processNextRequest();
    g_apServer.handleClient();
    M5.update();
    if (M5.BtnB.wasClicked()) break;
    delay(10);
  }

  g_apServer.stop();
  g_dnsServer.stop();
}

// Returns true = connected (flow finished). false = back to list / failed.
static bool apPortalRun() {
  char apName[24];
  buildApName(apName, sizeof(apName));

  if (!launcherWifiStartAp(apName, "", 0, 4)) {
    drawMsg("AP start failed");
    delay(1500);
    return false;
  }

  IPAddress apIP(192, 168, 4, 1);   // idf_wifi launcherWifiStartAp fixes the AP IP here

  g_dnsServer.start(53, "*", apIP);
  g_apServer.on("/", handleApRoot);
  g_apServer.on("/wifi", HTTP_POST, handleApPost);
  g_apServer.onNotFound(handleApNotFound);
  g_apServer.begin();

  drawQrScreen(apName);

  launcherWifiStop();          // AP off; STA takes over below
  if (!g_apSaved) return false;

  String ssid, pass;
  wifiLoadCredentials(ssid, pass);
  drawMsg("Connecting...", ssid.c_str());
  if (launcherWifiConnect(ssid.c_str(), pass.c_str(), 15000)) {
    if (syncNTP()) g_timeSynced = true;
    drawMsg(g_timeSynced ? "Time synced!" : "WiFi connected", ssid.c_str());
    delay(1000);
    return true;
  }
  drawMsg("Connect failed", "Check phone entry");
  delay(2000);
  return false;
}

// ── Main WiFi setup flow ──
bool wifiRunSetupFlow() {
  drawMsg("Scanning WiFi...");
  delay(500);

  std::vector<LauncherWifiAp> aps;
  int n = launcherWifiScan(aps);
  if (n <= 0) {
    drawMsg("No networks found", "Try again later");
    delay(2000);
    launcherWifiStop();   // 扫描时射频已开, 失败退出也必须关 (否则耗电到重启)
    return false;
  }

  int sel = pickSSID(aps);
  if (sel == -2) return apPortalRun();
  if (sel < 0)   { launcherWifiStop(); return false; }

  String ssid = aps[sel].ssid.c_str();
  bool openNet = (aps[sel].authmode == WIFI_AUTH_OPEN);

  String pass;
  if (!openNet) {
    char prompt[48];
    snprintf(prompt, sizeof(prompt), "Password for %.30s", ssid.c_str());
    if (!textInput(prompt, pass, 64)) { launcherWifiStop(); return false; }
  }

  drawMsg("Connecting...", ssid.c_str());
  delay(300);

  if (launcherWifiConnect(ssid.c_str(), pass.c_str(), 15000)) {
    wifiSaveCredentials(ssid, pass);
    drawMsg("WiFi connected!", ssid.c_str());
    delay(1000);

    if (syncNTP()) {
      g_timeSynced = true;
      drawMsg("Time synced!");
    } else {
      drawMsg("NTP failed", "Time may be wrong");
    }
    delay(1000);
    return true;
  } else {
    auto status = launcherWifiConnectStatus(ssid.c_str(), pass.c_str(), 5000);
    if (status == LauncherWifiConnectState::WrongPassword) {
      drawMsg("Wrong password!", "Try again");
    } else {
      char buf[40]; snprintf(buf, sizeof(buf), "%.38s", ssid.c_str());
      drawMsg("Connection failed", buf);
    }
    launcherWifiStop();
    delay(2000);
    return false;
  }
}

#else  // non-esp32 stubs (build safety)
bool wifiRunSetupFlow() { return false; }
#endif

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
// 与 main.cpp 里旧的 WiFi.disconnect(true)+WiFi.mode(WIFI_OFF) 不同,
// 这里走 idf_wifi 自己的状态机, 避免 Arduino WiFiGeneric 与裸 esp_wifi
// 双栈状态脱节 (见走查 P0 #3)。
void wifiStopAll() {
  launcherWifiStop();
}
