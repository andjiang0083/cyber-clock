/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 andjiang0083
 */
// ── timesync.h — shared NTP sync declaration ──
#pragma once
#include <cstdint>

extern bool g_timeValid;     // set true by syncNTP
extern time_t g_epoch;       // NTP epoch
extern uint32_t g_epochBase; // millis() at sync time
extern int g_tzOffset;       // UTC offset in hours (NVS persisted)

// Returns true on success, also sets g_timeValid
bool syncNTP();
