// ────────────────────────────────────────────────────────────
// wifisetup.h — WiFi configuration flow for cyber-clock
// Uses idf_wifi (port from bmorcelli/Launcher) for scanning,
// connecting, and AP mode.
// ────────────────────────────────────────────────────────────
#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Call once at boot. Returns true if WiFi is configured and connected.
bool wifiSetupBegin(bool forceReconfig = false);

// Returns true when time was synced via NTP.
bool wifiTimeSynced();

// Stop WiFi radio (esp_wifi layer) — unified shutdown path.
// Replaces ad-hoc WiFi.disconnect(true)+WiFi.mode(WIFI_OFF) from main.cpp.
void wifiStopAll();

// Store/load credentials from NVS
bool wifiLoadCredentials(String& ssid, String& pass);
void wifiSaveCredentials(const String& ssid, const String& pass);
void wifiClearCredentials();

// Scan → pick → password → connect flow.
// Returns true on successful connection.
bool wifiRunSetupFlow();
