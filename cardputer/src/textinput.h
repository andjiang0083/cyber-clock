/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 andjiang0083
 */
// ────────────────────────────────────────────────────────────
// textinput.h — StickS3 2-key on-screen text input widget
// Ported from bmorcelli/Launcher mykeyboard.cpp concept,
// adapted for 240×135 landscape with 2-button navigation.
// ────────────────────────────────────────────────────────────
#pragma once
#include <Arduino.h>
#include <WString.h>

// Callback: returns true when the user confirms text.
// Result is written into `out` buffer.
bool textInput(const char* prompt, String& out, int maxLen = 32);
