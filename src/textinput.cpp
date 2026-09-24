// ────────────────────────────────────────────────────────────
// textinput.cpp — StickS3 2-key on-screen text input
// ────────────────────────────────────────────────────────────
#include "textinput.h"
#include <M5Unified.h>

// ── Character set ──
static const char CHARS[] =
    " 0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "!@#$%^&*()_+-=[]{}\\|;:'\",.<>/?~`";
static const int N_CHARS = sizeof(CHARS) - 1;

// Special control codes embedded in the ring
enum Cmd : int {
  CMD_DEL  = -1,
  CMD_SPC  = -2,
  CMD_OK   = -3,
  CMD_EXIT = -4,
};

// ── Draw helpers (direct display, no sprite) ──
static void drawText(int x, int y, const char* s, uint16_t c, float sz = 1) {
  M5.Display.setTextSize(sz);
  M5.Display.setTextColor(c, TFT_BLACK);
  M5.Display.setCursor(x, y);
  M5.Display.print(s);
}

// ── Text input main ──
bool textInput(const char* prompt, String& out, int maxLen) {
  int cPos = 0;          // index into CHARS
  bool done = false;
  bool ok   = false;
  bool redraw = true;
  uint32_t lastBtn = 0;

  // Control actions that show first in the ring
  // We show them as labelled items preceding CHARS
  const int N_CMDS = 4;
  const char* CMD_LABELS[N_CMDS] = { "DEL", "SPC", "OK ", "EXT" };
  const int CMD_VALS[N_CMDS]     = { CMD_DEL, CMD_SPC, CMD_OK, CMD_EXIT };

  // When cPos < N_CMDS, we're on a command; otherwise on CHARS[cPos - N_CMDS]
  const int TOTAL_ITEMS = N_CMDS + N_CHARS;
  const int VISIBLE     = 10; // chars shown per row
  const int CW          = 24; // char cell width in px

  while (!done) {
    uint32_t now = millis();

    // ── Button handling ──
    M5.update();
    if (M5.BtnA.wasClicked()) {
      // Next item
      cPos = (cPos + 1) % TOTAL_ITEMS;
      redraw = true;
      lastBtn = now;
    }
    if (M5.BtnA.pressedFor(600) && now - lastBtn > 800) {
      // Long A = previous item (auto-repeat)
      cPos = (cPos - 1 + TOTAL_ITEMS) % TOTAL_ITEMS;
      redraw = true;
      lastBtn = now;
    }
    if (M5.BtnB.wasClicked()) {
      // Select current item
      if (cPos < N_CMDS) {
        int cmd = CMD_VALS[cPos];
        switch (cmd) {
          case CMD_DEL:
            if (out.length() > 0) out.remove(out.length() - 1);
            break;
          case CMD_SPC:
            if (out.length() < maxLen) out += ' ';
            break;
          case CMD_OK:
            ok = true; done = true; break;
          case CMD_EXIT:
            ok = false; done = true; break;
        }
      } else {
        char ch = CHARS[cPos - N_CMDS];
        if (out.length() < maxLen) out += ch;
      }
      redraw = true;
      lastBtn = now;
    }
    if (M5.BtnB.pressedFor(600) && now - lastBtn > 800) {
      // Long B = OK (confirm)
      ok = true; done = true; break;
    }

    // ── Render ──
    if (!redraw) { delay(30); continue; }
    redraw = false;

    M5.Display.fillScreen(TFT_BLACK);

    // Prompt line
    drawText(2, 2, prompt, M5.Display.color565(100, 100, 100), 1);

    // Input text line
    String display = out;
    if (out.length() > 20) display = "..." + out.substring(out.length() - 17);
    drawText(2, 14, display.c_str(), M5.Display.color565(200, 200, 200), 1);

    // ── Character row ──
    int cx = 2, cy = 50;
    int halfV = VISIBLE / 2;
    int startIdx = cPos - halfV;
    // Clamp so we always show VISIBLE items (wrapping)
    for (int i = 0; i < VISIBLE; i++) {
      int idx = (startIdx + i + TOTAL_ITEMS) % TOTAL_ITEMS;
      bool selected = (idx == cPos);

      // Draw cell background
      uint16_t bg = selected ? M5.Display.color565(0, 100, 200) : TFT_BLACK;
      M5.Display.fillRect(cx, cy, CW, 22, bg);
      M5.Display.drawRect(cx, cy, CW, 22, M5.Display.color565(60, 60, 60));

      // Draw character or command label
      const char* label = "";
      char single[2] = {0, 0};
      if (idx < N_CMDS) {
        label = CMD_LABELS[idx];
      } else {
        single[0] = CHARS[idx - N_CMDS];
        label = single;
      }

      uint16_t tc = selected ? TFT_WHITE : M5.Display.color565(150, 150, 150);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(tc, bg);
      int lw = strlen(label) * 6;
      M5.Display.setCursor(cx + (CW - lw) / 2, cy + 6);
      M5.Display.print(label);
      cx += CW + 1;
    }

    // ── Cursor indicator ──
    drawText(110, 78, "__", M5.Display.color565(0, 150, 255), 1);

    // ── Instruction line ──
    drawText(2, 95, "KEY1:scroll  KEY2:pick  LONG:OK", M5.Display.color565(80, 80, 80), 1);

    delay(30);
  }

  return ok;
}
