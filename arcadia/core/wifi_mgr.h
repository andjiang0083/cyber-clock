// ============================================================
// core/wifi_mgr.h — Arcadia WiFi 配网管理器
// AP配网 · 二维码 · WiFi扫描 · 键盘输入 · NVS存储
//
// 配网流程:
//   开机 → 检测 NVS 中的 WiFi 配置
//     ├─ 有配置 → 尝试连接 → 成功 → 返回 true
//     │                      └─ 失败 → AP配网模式
//     └─ 无配置 → AP配网模式
//                 ├─ AP: "Arcadia-XXXX" → 手机连接
//                 ├─ 显示二维码 (JSON)
//                 └─ 或手动: WiFi列表 → 选择 → 键盘输入密码
//
// 用法:
//   ArcWiFiMgr wifi;
//   wifi.begin();
//   if (wifi.isConnected()) { /* 联网成功 */ }
//   void loop() { wifi.update(); }  // 持续配网直到成功
// ============================================================
#pragma once
#include <cstdint>
#include <cstring>
#include <WiFi.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <WebServer.h>

// ===================== QR 码支持 =====================
// M5GFX 内置 QR 码库 (lgfx_qrcode.h)
// 在 arcadia.h 中通过 display.h 间接获得 M5GFX 上下文
#include <lgfx/utility/lgfx_qrcode.h>

// ===================== 配网状态 =====================
enum ArcWiFiState : uint8_t {
    ARC_WIFI_INIT,           // 初始
    ARC_WIFI_LOADING_NVS,    // 读取 NVS
    ARC_WIFI_CONNECTING,     // 尝试连接
    ARC_WIFI_CONNECTED,      // 连接成功
    ARC_WIFI_FAILED,         // 连接失败
    ARC_WIFI_AP_MODE,        // AP 配网模式
    ARC_WIFI_SCANNING,       // 扫描 WiFi
    ARC_WIFI_SCAN_DONE,      // 扫描完成
    ARC_WIFI_INPUT_SSID,     // 输入 SSID
    ARC_WIFI_INPUT_PWD,      // 输入密码
    ARC_WIFI_DONE,           // 配网完成
};

// ===================== 键盘输入缓冲区 =====================
struct ArcInputBuffer {
    char buf[64];
    int len = 0;
    int cursor = 0;

    void clear() { len = 0; cursor = 0; buf[0] = '\0'; }

    // 插入字符
    bool add(char c) {
        if (len >= (int)sizeof(buf) - 2) return false;
        buf[len++] = c;
        buf[len] = '\0';
        cursor = len;
        return true;
    }

    // 退格
    bool backspace() {
        if (len <= 0) return false;
        len--;
        buf[len] = '\0';
        cursor = len;
        return true;
    }

    const char* text() const { return buf; }
    bool empty() const { return len == 0; }
};

// ===================== WiFi 扫描结果 =====================
struct ArcWiFiAP {
    char ssid[33];
    int rssi;
    uint8_t encryptionType;
    bool active;  // 当前正在连接的 AP
};

static const int ARC_MAX_APS = 20;

// ===================== WiFi 管理器 =====================
struct ArcWiFiMgr {
    ArcWiFiState state = ARC_WIFI_INIT;
    char _ssid[32] = "";
    char _password[64] = "";
    bool _connected = false;
    bool _apMode = false;
    uint32_t _lastScan = 0;
    uint32_t _connStart = 0;
    int _scanResult = 0;
    ArcWiFiAP _aps[ARC_MAX_APS];
    int _apCount = 0;
    int _selectedAP = -1;
    ArcInputBuffer _inputBuf;
    bool _inputPasswordMode = false;
    WebServer* _server = nullptr;
    int _qrVersion = 0;

    // ===================== NVS key =====================
    static const char* NVS_NAMESPACE;
    static const char* NVS_KEY_SSID;
    static const char* NVS_KEY_PWD;

    // ===================== 初始化 =====================
    void begin() {
        state = ARC_WIFI_LOADING_NVS;
        // 从 NVS 读取
        if (loadFromNVS()) {
            state = ARC_WIFI_CONNECTING;
            _connStart = millis();
            WiFi.begin(_ssid, _password);
        } else {
            startAPMode();
        }
    }

    // ===================== 每帧更新 =====================
    void update() {
        switch (state) {
        case ARC_WIFI_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                _connected = true;
                state = ARC_WIFI_CONNECTED;
            } else if (millis() - _connStart > 15000) {
                // 超时 → 进入 AP 配网
                WiFi.disconnect();
                startAPMode();
            }
            break;

        case ARC_WIFI_AP_MODE:
            if (_server) _server->handleClient();
            break;

        case ARC_WIFI_SCANNING:
            checkScanComplete();
            break;

        default:
            break;
        }
    }

    // ===================== AP 配网模式 =====================
    void startAPMode() {
        _apMode = true;
        state = ARC_WIFI_AP_MODE;

        // 生成唯一 AP 名
        char apName[32];
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(apName, sizeof(apName), "Arcadia-%02X%02X", mac[4], mac[5]);

        WiFi.mode(WIFI_AP);
        WiFi.softAP(apName, "12345678");  // 默认密码

        // 启动 WebServer
        if (!_server) {
            _server = new WebServer(80);
            _server->on("/", HTTP_GET, [this]() {
                String html = "<html><body><h2>Arcadia WiFi Setup</h2>"
                    "<form method='POST' action='/save'>"
                    "SSID: <input name='ssid'><br>"
                    "Password: <input name='pwd' type='password'><br>"
                    "<input type='submit' value='Connect'>"
                    "</form></body></html>";
                _server->send(200, "text/html", html);
            });
            _server->on("/save", HTTP_POST, [this]() {
                String ssid = _server->arg("ssid");
                String pwd  = _server->arg("pwd");
                ssid.toCharArray(_ssid, sizeof(_ssid));
                pwd.toCharArray(_password, sizeof(_password));
                _server->send(200, "text/plain", "OK, connecting...");
                delay(100);
                saveToNVS();
                _apMode = false;
                WiFi.mode(WIFI_STA);
                state = ARC_WIFI_CONNECTING;
                _connStart = millis();
                WiFi.begin(_ssid, _password);
            });
            _server->begin();
        }
    }

    // ===================== 二维码显示 =====================
    // 在 canvas 上绘制 QR 码
    // 编码内容: {"s":"Arcadia","ssid":"xxx","pwd":"12345678","ip":"192.168.4.1"}
    void drawQR(int x, int y, int size) {
        char qrData[256];
        IPAddress ip = WiFi.softAPIP();
        snprintf(qrData, sizeof(qrData),
            "{\"s\":\"Arcadia\",\"ip\":\"%d.%d.%d.%d\"}",
            ip[0], ip[1], ip[2], ip[3]);

        // 使用 M5GFX QR 码库
        uint8_t qrVersion = 4;  // 版本 4 = 33×33 模块
        int bufSize = lgfx_qrcode_getBufferSize(qrVersion);
        uint8_t* qrBuf = (uint8_t*)malloc(bufSize);
        if (!qrBuf) return;

        QRCode qr;
        if (lgfx_qrcode_initText(&qr, qrBuf, qrVersion, ECC_LOW, qrData) == 0) {
            int moduleSize = size / qr.size;
            int offsetX = x + (size - qr.size * moduleSize) / 2;
            int offsetY = y + (size - qr.size * moduleSize) / 2;

            auto& c = ArcDisplay::canvas;
            // 白底
            c.fillRect(x, y, size, size, TFT_WHITE);

            // 画 QR 码
            for (int row = 0; row < qr.size; row++) {
                for (int col = 0; col < qr.size; col++) {
                    if (lgfx_qrcode_getModule(&qr, col, row)) {
                        int px = offsetX + col * moduleSize;
                        int py = offsetY + row * moduleSize;
                        c.fillRect(px, py, moduleSize, moduleSize, TFT_BLACK);
                    }
                }
            }
        }
        free(qrBuf);
    }

    // ===================== WiFi 扫描 =====================
    void startScan() {
        state = ARC_WIFI_SCANNING;
        _apCount = 0;
        _scanResult = WiFi.scanNetworks(true, false, false, 250);
        _lastScan = millis();
    }

    void checkScanComplete() {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            _apCount = 0;
            for (int i = 0; i < n && i < ARC_MAX_APS; i++) {
                strncpy(_aps[_apCount].ssid, WiFi.SSID(i).c_str(), 32);
                _aps[_apCount].ssid[31] = '\0';
                _aps[_apCount].rssi = WiFi.RSSI(i);
                _aps[_apCount].encryptionType = WiFi.encryptionType(i);
                _aps[_apCount].active = false;
                _apCount++;
            }
            WiFi.scanDelete();
            state = ARC_WIFI_SCAN_DONE;
        } else if (n == WIFI_SCAN_FAILED) {
            WiFi.scanDelete();
            state = ARC_WIFI_SCAN_DONE;
        }
    }

    // ===================== 绘制扫描列表 =====================
    void drawAPList(int x, int y, int lineH, int selected) {
        auto& c = ArcDisplay::canvas;
        c.setFont(&fonts::Font0);
        c.setTextSize(1);

        for (int i = 0; i < _apCount && i < 8; i++) {
            int cy = y + i * lineH;
            if (i == selected) {
                c.fillRect(x - 1, cy - 1, ArcDisplay::W - 2, lineH, c565(20, 30, 50));
                c.setTextColor(ArcColor::HUD_ACCENT, c565(20, 30, 50));
            } else {
                c.setTextColor(ArcColor::HUD_MAIN, TFT_BLACK);
            }

            // RSSI 强度指示
            char rssiSym = (_aps[i].rssi > -60) ? 'S' : (_aps[i].rssi > -80) ? 's' : '.';
            c.setCursor(x, cy);
            c.printf(" %c %s", rssiSym, _aps[i].ssid);
        }
    }

    // ===================== 键盘输入 =====================
    // 处理按键输入, 返回 true 表示确认完成
    bool handleInput(int key) {
        if (key == '\n' || key == '\r') {
            // 确认
            return true;
        } else if (key == '\b') {
            _inputBuf.backspace();
        } else if (key >= 0x20 && key <= 0x7E) {
            _inputBuf.add((char)key);
        }
        return false;
    }

    // 绘制输入框
    void drawInputBox(int x, int y, int w, const char* label, bool passwordMode) {
        auto& c = ArcDisplay::canvas;
        c.setFont(&fonts::Font0);
        c.setTextSize(1);

        // 标签
        c.setTextColor(ArcColor::HUD_MAIN, TFT_BLACK);
        c.setCursor(x, y - 10);
        c.print(label);

        // 输入框
        c.drawRect(x, y, w, 16, c565(80, 100, 140));

        // 文本 (密码模式用 *)
        c.setTextColor(ArcColor::HUD_ACCENT, TFT_BLACK);
        c.setCursor(x + 2, y + 3);
        if (passwordMode) {
            for (int i = 0; i < _inputBuf.len; i++) c.print('*');
        } else {
            c.print(_inputBuf.text());
        }

        // 光标
        if (millis() % 800 < 400) {
            int cx = x + 2 + (_inputBuf.len * 6);
            if (cx < x + w - 2)
                c.drawFastVLine(cx, y + 2, 12, ArcColor::HUD_ACCENT);
        }
    }

    // ===================== NVS 存储 =====================
    bool loadFromNVS() {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
        if (err != ESP_OK) return false;

        size_t len = sizeof(_ssid);
        err = nvs_get_str(handle, NVS_KEY_SSID, _ssid, &len);
        if (err != ESP_OK) { nvs_close(handle); return false; }

        len = sizeof(_password);
        err = nvs_get_str(handle, NVS_KEY_PWD, _password, &len);
        nvs_close(handle);

        return err == ESP_OK && strlen(_ssid) > 0;
    }

    void saveToNVS() {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) return;

        nvs_set_str(handle, NVS_KEY_SSID, _ssid);
        nvs_set_str(handle, NVS_KEY_PWD, _password);
        nvs_commit(handle);
        nvs_close(handle);
    }

    void clearNVS() {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) return;
        nvs_erase_key(handle, NVS_KEY_SSID);
        nvs_erase_key(handle, NVS_KEY_PWD);
        nvs_commit(handle);
        nvs_close(handle);
    }

    // ===================== 查询状态 =====================
    bool isConnected() const { return _connected; }
    bool isAPMode() const { return _apMode; }
    const char* getSSID() const { return _ssid; }
    int getAPCount() const { return _apCount; }
    ArcWiFiAP getAP(int i) const { return (i >= 0 && i < _apCount) ? _aps[i] : ArcWiFiAP{}; }
    ArcWiFiState getState() const { return state; }

    const char* getStateName() const {
        switch (state) {
            case ARC_WIFI_INIT:         return "INIT";
            case ARC_WIFI_LOADING_NVS:  return "LOADING";
            case ARC_WIFI_CONNECTING:   return "CONNECTING...";
            case ARC_WIFI_CONNECTED:    return "CONNECTED";
            case ARC_WIFI_FAILED:       return "FAILED";
            case ARC_WIFI_AP_MODE:      return "AP MODE";
            case ARC_WIFI_SCANNING:     return "SCANNING...";
            case ARC_WIFI_SCAN_DONE:    return "SCAN DONE";
            case ARC_WIFI_INPUT_SSID:   return "INPUT SSID";
            case ARC_WIFI_INPUT_PWD:    return "INPUT PWD";
            case ARC_WIFI_DONE:         return "DONE";
            default: return "UNKNOWN";
        }
    }
};

// 静态成员定义
const char* ArcWiFiMgr::NVS_NAMESPACE = "arcadia";
const char* ArcWiFiMgr::NVS_KEY_SSID  = "wifi_ssid";
const char* ArcWiFiMgr::NVS_KEY_PWD   = "wifi_pwd";
