// ────────────────────────────────────────────────────────────
// textinput.cpp — StickS3 2-key on-screen text input
// v1.1.2: big carousel (ported from stick-cook v0.2.2) — current
// char size=6 centered, neighbors ±1 at size=3 and ±2 at size=2 so
// the user can see exactly what sits next to the selection before
// pressing KEY2. No cell boxes. Direct-to-display (no sprite).
// ────────────────────────────────────────────────────────────
#include "textinput.h"
#include <M5Unified.h>

// ── Character set (ring order) ──
static const char CHARS[] =
    " 0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "!@#$%^&*()_+-=[]{}\\|;:'\",.<>/?~`";
static const int N_CHARS = sizeof(CHARS) - 1;

// Control commands shown at the ring head
enum Cmd : int {
  CMD_DEL  = -1,
  CMD_SPC  = -2,
  CMD_OK   = -3,
  CMD_EXIT = -4,
};

static const int N_CMDS = 4;
static const char* CMD_LABELS[N_CMDS] = { "DEL", "SPC", "OK", "EXIT" };
static const int CMD_VALS[N_CMDS]     = { CMD_DEL, CMD_SPC, CMD_OK, CMD_EXIT };

// label for a ring index (command name or single char)
static const char* itemLabel(int idx, char* buf1) {
  if (idx < N_CMDS) return CMD_LABELS[idx];
  buf1[0] = CHARS[idx - N_CMDS];
  buf1[1] = '\0';
  return buf1;
}

static void renderInput(int cPos, const String& out, int maxLen, const char* prompt) {
  auto& d = M5.Display;
  const int TOTAL = N_CMDS + N_CHARS;

  // Top row: prompt left, ring counter right (size=1)
  char pbuf[48];
  strncpy(pbuf, prompt ? prompt : "", 36); pbuf[36] = '\0';
  d.setTextSize(1);
  d.setTextColor(M5.Display.color565(140, 140, 150));
  d.setCursor(2, 2);
  d.print(pbuf);
  char cbuf[16];
  snprintf(cbuf, sizeof(cbuf), "%d/%d", cPos + 1, TOTAL);
  d.setTextColor(M5.Display.color565(80, 90, 100));
  d.setCursor(236 - (int)strlen(cbuf) * 6, 2);
  d.print(cbuf);

  // Input line: plaintext tail size=2 + cursor block
  String shown = out;
  if ((int)shown.length() > 19) shown = "..." + shown.substring(shown.length() - 16);
  d.setTextSize(2);
  d.setTextColor(M5.Display.color565(210, 210, 220));
  d.setCursor(3, 14);
  int tx = 3 + (int)shown.length() * 12;
  d.print(shown.c_str());
  if ((int)out.length() < maxLen) {
    d.fillRect(tx + 2, 17, 8, 13, M5.Display.color565(0, 200, 255));
  }

  // ── Carousel row: [±2] [±1] CENTER [±1] [±2], bottom-aligned ──
  const int yBase = 98;
  struct Slot { int idx; int sz; uint16_t col; };
  Slot slots[5];
  char b1[2];
  for (int off = -2; off <= 2; off++) {
    int idx = (cPos + off + TOTAL * 2) % TOTAL;
    bool cmd = idx < N_CMDS;
    int sz; uint16_t col;
    if (off == 0)        { sz = cmd ? 3 : 6; col = TFT_WHITE; }
    else if (abs(off)==1){ sz = cmd ? 2 : 3; col = M5.Display.color565(190, 190, 200); }
    else                 { sz = cmd ? 1 : 2; col = M5.Display.color565(90, 100, 112); }
    slots[off + 2] = { idx, sz, col };
  }
  int widths[5];
  int totalW = 0, gap = 8;
  for (int i = 0; i < 5; i++) {
    widths[i] = (int)(strlen(itemLabel(slots[i].idx, b1)) * 6.0f * slots[i].sz);
    totalW += widths[i];
  }
  totalW += gap * 4;
  if (totalW > 236) {
    gap = 4;
    totalW -= (8 - 4) * 4;
  }
  int x = (240 - totalW) / 2;
  for (int i = 0; i < 5; i++) {
    const Slot& s = slots[i];
    d.setTextSize(s.sz);
    d.setTextColor(s.col);
    int top = yBase - (int)(8.0f * s.sz) + 1;
    d.setCursor(x, top);
    d.print(itemLabel(s.idx, b1));
    if (i == 2) {   // selection: subtle underline instead of a box
      d.fillRect(x, yBase + 1, widths[i], 2, M5.Display.color565(0, 180, 255));
    }
    x += widths[i] + gap;
  }

  // Hints (size=1)
  d.setTextSize(1);
  d.setTextColor(M5.Display.color565(140, 140, 150));
  d.setCursor((240 - 23 * 6) / 2, 108);
  d.print("KEY1:scroll   KEY2:add");
  d.setTextColor(M5.Display.color565(80, 90, 100));
  d.setCursor((240 - 25 * 6) / 2, 122);
  d.print("HOLD B: connect now");
}

// ── Text input main ──
bool textInput(const char* prompt, String& out, int maxLen) {
  const int TOTAL = N_CMDS + N_CHARS;
  int cPos = 0;          // ring index (commands first, then chars)
  bool done = false;
  bool ok   = false;
  bool redraw = true;
  uint32_t lastBtn = 0;

  while (!done) {
    uint32_t now = millis();

    // ── Buttons: A=next (hold=back), B=add / hold=OK ──
    M5.update();
    if (M5.BtnA.wasClicked()) {
      cPos = (cPos + 1) % TOTAL;
      redraw = true; lastBtn = now;
    }
    if (M5.BtnA.pressedFor(600) && now - lastBtn > 800) {
      cPos = (cPos - 1 + TOTAL) % TOTAL;    // hold A scrolls backwards
      redraw = true; lastBtn = now + 400;   // slow auto-repeat
    }
    if (M5.BtnB.wasClicked()) {
      if (cPos < N_CMDS) {
        switch (CMD_VALS[cPos]) {
          case CMD_DEL:  if (out.length() > 0) out.remove(out.length() - 1); break;
          case CMD_SPC:  if ((int)out.length() < maxLen) out += ' '; break;
          case CMD_OK:   ok = true; done = true; break;
          case CMD_EXIT: ok = false; done = true; break;
        }
      } else {
        if ((int)out.length() < maxLen) out += CHARS[cPos - N_CMDS];
      }
      redraw = true; lastBtn = now;
    }
    if (M5.BtnB.pressedFor(800) && now - lastBtn > 1000) {
      ok = true; done = true;                // hold B = connect now
    }

    if (!redraw) { delay(30); continue; }
    redraw = false;

    M5.Display.fillScreen(TFT_BLACK);
    renderInput(cPos, out, maxLen, prompt);
    delay(30);
  }

  return ok;
}
