// ────────────────────────────────────────────────────────────
// textinput.cpp — Cardputer 物理键盘输入
// 替代 S3 的屏幕键盘, 直接用 M5Cardputer 键盘输入
// ────────────────────────────────────────────────────────────
#include "textinput.h"
#include <M5Cardputer.h>

// 将按键转为可见字符 (忽略修饰键)
static char keyToChar(char k) {
  if (k >= 'a' && k <= 'z') return k;     // 小写字母
  if (k >= 'A' && k <= 'Z') return k;     // 大写字母
  if (k >= '0' && k <= '9') return k;     // 数字
  // 常用符号
  const char* symbols = " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
  for (const char* p = symbols; *p; p++) if (k == *p) return k;
  return 0; // 忽略其他
}

bool textInput(const char* prompt, String& out, int maxLen) {
  out = "";
  bool done = false;

  // 清屏一次
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.startWrite();

  while (!done) {
    M5Cardputer.update();

    // 输入框背景 (仅重绘输入区减少闪烁)
    M5.Display.fillRect(4, 22, 232, 20, TFT_BLACK);

    // 提示
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.print(prompt);

    // 输入框边框
    M5.Display.drawRect(4, 22, 232, 20, M5.Display.color565(80, 80, 80));

    // 明文显示已输入的字符
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setCursor(8, 24);
    M5.Display.print(out);

    // 光标
    if ((millis() / 400) % 2 == 0) {
      uint16_t cx = 8 + out.length() * 6;
      if (cx < 232) {
        M5.Display.drawFastVLine(cx, 24, 16, TFT_WHITE);
      }
    }

    // 底部提示
    M5.Display.fillRect(4, 118, 232, 12, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(120, 120, 120), TFT_BLACK);
    M5.Display.setCursor(4, 118);
    M5.Display.print("Enter=OK  Del=BS  Esc=Cancel");

    // 键盘输入
    if (M5Cardputer.Keyboard.isChange()) {
      auto ks = M5Cardputer.Keyboard.keysState();

      if (ks.del) {
        if (out.length() > 0) out.remove(out.length() - 1);
      } else if (ks.enter) {
        done = true;
        break;
      } else {
        for (auto key : ks.word) {
          if (key == 0x1b || key == 27) return false;
          char c = keyToChar(key);
          if (c && (int)out.length() < maxLen) out += c;
        }
      }
    }

    delay(20);
  }

  M5.Display.endWrite();
  return out.length() > 0;
}
