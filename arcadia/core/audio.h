// ============================================================
// core/audio.h — Arcadia 音频系统
// 音效反馈 · 背景音 · 摩斯密码引擎
//
// 用法:
//   arcClick();          // 短促点击 (交互反馈)
//   arcBeep();           // 确认提示音
//   arcError();          // 错误提示音
//   arcTone(1047, 120);  // 自定义音
//
//   摩斯引擎:
//   ArcMorse morse("SOS");
//   morse.update();      // 每帧调用, 自动播放
//   morse.setOn(true/false);
// ============================================================
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>

// ===================== 基本音效 =====================
// 短点击 (用于交互反馈, 如换页/切换)
static inline void arcClick() {
    M5.Speaker.tone(1800, 8, 40);
}

// 确认提示 (两声)
static inline void arcBeep() {
    M5.Speaker.tone(1400, 12, 50);
    delay(15);
    M5.Speaker.tone(1800, 12, 50);
}

// 错误提示 (低沉)
static inline void arcError() {
    M5.Speaker.tone(400, 30, 60);
    delay(35);
    M5.Speaker.tone(300, 40, 60);
}

// 自定义音调
static inline void arcTone(int freq, int dur, int vol = 50) {
    M5.Speaker.tone(freq, dur, vol);
}

// ===================== 摩斯密码引擎 =====================
// 从 threebody 项目中提取的状态机
// 支持 A-Z, 0-9, 空格
struct ArcMorse {
    // 摩斯码表
    static const char* CODE[36]; // A-Z + 0-9

    const char* _msg;        // 消息字符串
    int _msgLen = 0;         // 消息长度
    bool _on = true;         // 是否启用

    // 状态机
    enum State { WAIT, PLAY_ELEM, DONE } _state = WAIT;
    int _charIdx = 0;        // 消息中当前字符
    int _elemIdx = 0;        // 当前摩斯码中的元素
    const char* _code = "";  // 当前字符的摩斯码
    uint32_t _nextTime = 0;  // 下次动作时间

    // 定时参数 (可自定义)
    int dotMs = 120;         // 点时长
    int dashMs = 360;        // 划时长
    int elemGap = 120;       // 元件间隔
    int charGap = 360;       // 字母间隔 (=3×dot)
    int wordGap = 840;       // 单词间隔 (=7×dot)
    int msgPause = 20000;    // 消息播完后静默再重复

    // 频率
    int dotFreq = 1047;      // 点=高音 C6
    int dashFreq = 784;      // 划=低音 G5

    ArcMorse() {}

    // 设置消息
    void setMessage(const char* msg) {
        _msg = msg;
        _msgLen = strlen(msg);
        reset();
    }

    // 重置到开头
    void reset() {
        _state = WAIT;
        _charIdx = 0;
        _elemIdx = 0;
        _code = "";
        _nextTime = millis() + 2000;  // 2s 后开始
    }

    // 启用/禁用
    void setOn(bool on) {
        _on = on;
        if (!on) M5.Speaker.stop();
    }
    bool isOn() const { return _on; }
    void toggle() { setOn(!_on); }

    // ===================== 每帧调用 =====================
    void update() {
        if (!_on) return;
        uint32_t now = millis();
        if (now < _nextTime) return;

        if (_state == WAIT) {
            char c = _msg[_charIdx];
            if (c == ' ') {
                // 单词间隔
                _nextTime = now + wordGap;
                _charIdx++;
                if (_charIdx >= _msgLen) _state = DONE;
                return;
            }
            // 查码
            if (c >= 'A' && c <= 'Z') _code = CODE[c - 'A'];
            else if (c >= 'a' && c <= 'z') _code = CODE[c - 'a'];
            else if (c >= '0' && c <= '9') _code = CODE[26 + (c - '0')];
            else { _charIdx++; return; }

            _elemIdx = 0;
            _state = PLAY_ELEM;
        }

        if (_state == PLAY_ELEM) {
            char sym = _code[_elemIdx];
            bool isDot = (sym == '.');
            int dur = isDot ? dotMs : dashMs;
            int freq = isDot ? dotFreq : dashFreq;
            int vol = 22 + rand() % 10;
            M5.Speaker.tone(freq, dur, vol);

            _elemIdx++;
            if (_elemIdx >= (int)strlen(_code)) {
                _charIdx++;
                if (_charIdx >= _msgLen) {
                    _nextTime = now + dur + 100;
                    _state = DONE;
                } else {
                    _nextTime = now + dur + charGap;
                    _state = WAIT;
                }
            } else {
                _nextTime = now + dur + elemGap;
            }
            return;
        }

        if (_state == DONE) {
            _nextTime = now + msgPause + (rand() % 10000);
            _charIdx = 0;
            _state = WAIT;
        }
    }
};

// 摩斯码表实现
const char* ArcMorse::CODE[36] = {
    ".-","-...","-.-.","-..",".","..-.","--.","....","..",".---",  // A-J
    "-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-",    // K-T
    "..-","...-",".--","-..-","-.--","--..",                       // U-Z
    "-----",".----","..---","...--","....-",".....","-....","--...","---..","----."  // 0-9
};
