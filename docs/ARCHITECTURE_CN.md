# CYBER CLOCK — 架构说明

**中文** · [English](ARCHITECTURE.md)

固件是怎么搭起来的、为什么这么搭，以及那些改变了设计的真实 bug。动手改代码之前，建议先读对应章节——其中有几个决定在知道"当初坏在哪"之前是反直觉的。

---

## 1. 两个目标，一套程序

```
cyber-clock/
├── src/                StickS3 版        ─┐ 同一套程序，
├── cardputer/src/      Cardputer 版      ─┘ 两份拷贝
└── arcadia/core/       内嵌的 GUI / 输入 / IMU / 省电框架
```

两个应用都是单文件程序（`main.cpp`，约 800 行与 740 行），加上一个小型内嵌框架（`arcadia/core/`）和几个本地模块。`src/` 与 `cardputer/src/` 之间的重复是已知成本——它是[路线图](ROADMAP_CN.md)第 8 项——但这样每个目标都能独立构建，不需要共享组件的额外管线。

框架提供两个目标完全一致的部分：

| 头文件 | 提供什么 |
|---|---|
| `arcadia.h` | 一键包含 |
| `display.h` | `canvas` sprite、旋转、原子化 `push()` |
| `input.h` | `arcGetKey()` —— StickS3 双按键的统一短按/长按事件 |
| `color.h` | `c565()`、混色、色板 |
| `imu.h` | `ArcIMU` 互补滤波 |
| `powersave.h` | Light Sleep 辅助 |
| `screen.h`、`hud.h`、`audio.h`、`math3d.h`、`stars.h`、`app.h`、`autodemo.h` | 供其它项目使用；本项目引入无害 |

---

## 2. 渲染：Sprite 双缓冲，以及修掉黑屏的兜底

所有绘制先写进离屏 sprite，最后一次性推屏：

```cpp
ArcDisplay::begin(SCREEN_ROTATION, BRIGHTNESS_LEVELS[g_brightnessIdx]);  // 240×135 横屏
...
clear(TFT_BLACK);
drawMatrixRain();
drawTime();
drawDate();
drawBattery();
drawAudioBars();
push();                     // sprite → 面板，一次原子拷贝
```

没有 sprite 的话，逐图元绘制会让数字雨出现明显的撕裂。sprite 大小 240×135×2 = 64,800 字节，分配在 PSRAM。

**坑在这里：** `canvas.createSprite(240, 135)` 是可能返回 `false` 的（PSRAM 碎片、`M5.begin()` 调用顺序、旋转设置不当）。一旦失败，canvas 上的**所有**绘制调用会静默变成空操作，屏幕就停在最后一次 `fillScreen` 的状态——黑屏，不崩溃、不 panic、串口也没有任何输出。这正是 v0.1 的黑屏 bug。

**修法是加有效性标志，而不是重试循环：** sprite 创建失败时，渲染路径直接画到 `M5.Display`。慢一些，数字雨会有一点撕裂，但时钟依然可读——一块画面不够漂亮的钟，好过一块什么都不显示的钟。

```cpp
if (g_spriteOk) { ...画到 canvas...; push(); }
else            { ...直接画到 M5.Display...; }
```

两个相关细节：

- **`drawBattery()` 把 I²C 读取缓存 1 秒。** `M5.Power.getBatteryLevel()` 是一次 I²C 事务；33fps 下就是每秒 33 次事务，而电量一小时才动一格。
- **开机动画走直接绘制**（雨循环里的 `canvas.startWrite()` / `endWrite()`），这样在第一次完整渲染之前也能正常显示。

---

## 3. 时间

时钟在循环里从不调用 `getLocalTime()`。它这么做：

```cpp
syncNTP();                        // 设置 g_epoch (time_t) 与 g_epochBase = millis()
// 每帧：
epoch = g_epoch + (millis() - g_epochBase) / 1000;
localtime_r(&epoch, &tm);         // 时 / 分 / 秒 / 日期 / 星期
```

每帧只有一次整数加法和一次 `localtime`，没有 SNTP 轮询，漂移也只取决于 ESP32 自己的 RTC。

- NTP 服务器：`ntp.aliyun.com`、`ntp.tencent.com`、`pool.ntp.org`；20 次重试 × 250ms。
- 时区偏移存在 NVS（`cyberclock` 命名空间，键 `tz`），传给 `configTime(offset * 3600, 0, ...)`。它是**固定偏移**——真正的夏令时规则见[路线图第 4 项](ROADMAP_CN.md)。
- **如果 NTP 从未成功，`g_timeValid` 一直为 false，屏幕显示的是开机以来的计时。** 注意它对省电的连带影响：闲置状态机以 `g_timeValid` 为门槛，所以一块从没对上时的钟也不会进省电——这是刻意的，因为显示错误时间的屏幕，比烧着 30mA 的屏幕更糟。

---

## 4. 数字雨

55 颗雨滴，每颗有独立的 `x`、`y`、速度、头部字符、头部亮度，以及 5 字符的拖尾（含头部共 6 个字形）。

```
        head   ← 亮度 80…199，10% 概率 +60，上限 255
      trail[0] ← headBri / 5
      trail[1] ← headBri / 4
      trail[2] ← headBri / 3
      trail[3] ← headBri / 2
      trail[4] ← 5   （几乎看不见）
```

每个字形 6×8 px，拖尾按 `8px` 垂直间距绘制，尾端亮度下限为 5，这样拖尾不会出现硬边。拖尾颜色按 `c565(bri/8, bri/2, bri/4)` 计算——一条蓝绿色渐变。

**头部**颜色取决于当前字符集，这才是 `NUMBERS`、`BINARY`、`HEX`、`SYMBOLS`、`ALPHA` 看起来像不同特效、而不是同一串字符换了内容的原因：

| 字符集 | 头部颜色表达式 |
|---|---|
| `MATRIX` | `c565(bri/6, min(bri,120), bri/3)` —— 蓝绿 |
| `NUMBERS` | `c565(bri/10, min(bri,140), bri/10)` —— 纯绿 |
| `BINARY` | `c565(0, min(bri,200), bri/8)` —— 青 |
| `HEX` | `c565(bri/8, bri/2, min(bri,100))` —— 蓝紫 |
| `SYMBOLS` | `c565(min(bri,100), bri/2, bri/3)` —— 暖红 |
| `ALPHA` | `c565(bri/10, bri/3, min(bri,140))` —— 淡蓝 |

雨滴离开屏幕底部后会带着新的随机 `x` 和头部亮度重生，所以密度恒定，同一场开机内画面不会循环。随机种子是启动时的 `srand(esp_random())`，每次上电都不同。

**一个很阴的坑：枚举成员不能叫 `HEX`。**

```cpp
// Arduino 的 Print.h 里有:  #define HEX 16
enum class RainMode : uint8_t { MATRIX, NUMBER, BINARY, HEXADECIMAL, SYMBOL, ALPHA, MODES_N };
```

叫 `HEX` 会被预处理器替换成 `16`，然后在完全不相干的地方报一个看不懂的错。`HEXADECIMAL` 不是风格选择，是绕坑，源码注释里也写了这一点。

---

## 5. 输入

### StickS3 —— `arcGetKey()`

框架的输入层把两个物理按键变成四个事件：`'A'` / `'a'`（短按 / 长按）与 `'B'` / `'b'`。长按阈值用的是 M5Unified 的默认值 500ms（`Button_Class::_msecHold`），本项目没有覆盖它。`loop()` 里的映射就是一个普通 switch：

| 事件 | 动作 |
|---|---|
| `'A'` | 界面循环 |
| `'a'` | 下一个数字雨字符集 |
| `'B'` | `FULL`/`TIME` 下切主题，`SAVER` 下切字符集 |
| `'b'` | 下一档亮度 |

**v1.1.0 修掉了双触发。** 机制值得记下来：在 M5Unified 里，`wasHold()` 在按住跨过阈值的那一帧触发，而 `wasClicked()` 在**松手**那一帧触发——所以一次长按会产生两个事件，设备会把长按动作和短按动作都做一遍。修法在框架的输入层：长按事件之后 300ms 窗口内吞掉 `wasClicked()`，于是一次物理动作只产生一个事件。这也是 [CONTRIBUTING_CN.md](CONTRIBUTING_CN.md) 里专门列这一条的原因。

### Cardputer —— `getKey()`

键盘由 `M5Cardputer.Keyboard` 扫描，翻译成同一套语义：

- **符号键**（`+`、`-`、`;`、`.`）直接返回——它们无状态，天然不会重复。
- **字母键**用 26 位掩码做边沿检测：

  ```cpp
  mask |= 1UL << (k - 'a');
  edge  = mask & ~g_prevHeldMask;   // 只在按下的那一帧为真
  g_prevHeldMask = mask;
  ```

  没有这段的话，按住 `S` 会以每秒 33 次的速度循环切换界面。

两个目标共享同一套**语义**（`S`↔`A`、`C`↔`B`、`+`/`-`↔长按亮度），所以两份 `main.cpp` 读起来仍像同一个程序。

---

## 6. 麦克风频谱

底部 40 根彩条，每 50ms 取一块 160 采样：

- 第 `i` 根占 6px（`x = i * 6`），4px 条 + 2px 间隔；
- 色相铺满整个色轮：`hue = i * 240 / 40`；
- 亮度随幅度上升（`80 + h * 25`），形成暗底亮尖；
- 条足够高时（≥7）顶端像素闪白；
- 即使麦克风关闭，也会在 y=130/131 画一条 2px 基线，这样布局不会跳动。

`initAudio()` 在从省电唤醒后会重试（清除 `g_micFailed`），因为 ES8311 从 Light Sleep 回来后并不总是干净地恢复。如果麦克风始终起不来，频谱就干脆不显示——时钟不关心这件事。

---

## 7. IMU 摇一摇检测

`ArcIMU` 维护一个互补滤波的姿态估计（τ = 1.0s）。摇一摇检测看的不是原始加速度，而是**姿态角的变化率**：

```cpp
if (fabs(p - g_prevP) > SHAKE_THRESHOLD && fabs(r - g_prevR) > SHAKE_THRESHOLD) g_shakeCnt++;
else g_shakeCnt 衰减;
if (g_shakeCnt >= 3) { g_theme = random; g_shakeCnt = 0; }
```

阈值 0.15 弧度/帧，连续 3 帧。v1.1.0 修正了灵敏度——早先的版本在设备只是被拿起来的时候就会触发，那是没人要的主题切换。

---

## 8. 省电

每轮循环评估一次的三段式状态机：

```
                  任意输入 / 插入 USB
        ┌──────────────────────────────────────────────┐
        │                                              ▼
   ┌─────────┐  闲置 60s   ┌──────────┐  闲置 180s   ┌──────────────┐
   │ 正常    │────────────▶│ 降频     │─────────────▶│ LIGHT SLEEP  │
   │ 240MHz  │             │ 80MHz    │              │ 关背光        │
   │ ~33fps  │◀────────────│ 1fps     │◀─────────────│ 0.3mA 量级    │
   └─────────┘  任意输入   └──────────┘  任意输入    └──────────────┘
                                                       （仅 StickS3）
```

关键细节：

- **整个状态机以 `g_timeValid` 和 `!isCharging()` 为门槛。** 插上 USB 会取消所有档位、恢复 240MHz、恢复亮度、点亮绿色 LED 并重新初始化麦克风。
- **Light Sleep** 使用 200ms 的 RTC 定时器加 `GPIO_NUM_0`（`BtnA`）的 `ext0`：

  ```cpp
  esp_sleep_enable_timer_wakeup(SLEEP_POLL_US);          // 200ms
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);         // BtnA
  esp_light_sleep_start();
  ```

  唤醒后先查 `esp_sleep_get_wakeup_cause()`：`EXT0` 原因直接采信（那是真的按键）。否则先 `delay(5)`，再最多读 **5 次** `M5.update()` 才判定"没人按键"——`BtnB` 挂在 I²C 电源/IO 扩展芯片后面，唤醒后的头几毫秒并不可靠。
- **绿色 LED（GPIO24，低有效）** 在 Light Sleep 期间被拉高，恢复时拉低，免得睡着的钟在床头留一颗亮灯。
- **闲置渲染不是"照常画一帧然后丢掉"：** 在降频和 Light Sleep 两档，只有 `g_s`（秒）变化时才渲染，也就是 1fps。省电的大部分收益其实来自这里。
- **Cardputer 没有第三档。** 它的按键通过 TCA8418 I²C 扩展芯片扫描，没有 GPIO 唤醒线，所以 80MHz + 1fps 就是下限。见[路线图第 3 项](ROADMAP_CN.md)——那颗扩展芯片确实有 INT 输出，如果板上引出来了，这个问题也许有解。

### 试过又回退：IMU 动作唤醒

v0.1.2 曾用"IMU 检测到动作就唤醒"替代 Light Sleep，好让设备被拿起来时自动亮屏。紧接着的下一个提交就回退了：IMU 轮询本身就持续耗电，唤醒并不可靠，最终功耗比它想解决的问题还差。**按键唤醒是最终方案**——写在这里，免得有人重新发明一遍。

---

## 9. WiFi 配网

```
首次开机 / 按 [W] / 开机动画期间按住 BtnA
        │
        ▼
  NVS 里有凭据？ ──有──▶ 连接（超时） ──成功──▶ NTP 对时 ──▶ 关射频
        │没有                       │失败
        ▼                           ▼
   扫描网络 ──▶ 选 SSID ──▶ 输密码 ──▶ 连接
        │                                  │密码错误
        ▼                                  ▼
   （列表 UI）                      "Wrong password" → 重输
        │
        └──▶ 仅首次：选时区 ──▶ NTP 对时 ──▶ 关射频
```

| 部分 | 文件 | 说明 |
|---|---|---|
| WiFi 驱动层 | `src/idf/idf_wifi.cpp/.h` | 直接用 ESP-IDF `esp_wifi_*`（不走 Arduino `WiFi.h`），用事件组跟踪连接/断开，并通过断开原因判断密码错误。**派生自 [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher)** —— 见 [CREDITS.md](../CREDITS.md) |
| 配网流程 | `src/wifisetup.cpp/.h` | 扫描 → 列表 → 密码 → 连接 → 对时 → 存储 |
| 屏幕键盘（StickS3） | `src/textinput.cpp/.h` | 字符环：`BtnA` 滚动、`BtnB` 选中、长按 `BtnB` = OK。控制项 `DEL` / `SPC` / `OK` / `EXIT` 位于环首 |
| 存储 | `Preferences` 命名空间 `cyberclock` | `wifi_ssid`、`wifi_pass`、`tz` |

补充：

- **SSID 乱码：** 直接用原始 `uint8_t` SSID 字段构造 `std::string` 会得到乱码，因为该字段不是 NUL 结尾的。修法是 `strnlen` 加显式长度——也说明了为什么 SSID 要拷贝而不是直接当字符串打印。
- **非 ASCII SSID** 显示为 `□`：GLCD 字体没有对应字形。
- **对时之后关闭射频**（`wifiStopAll()`），此后靠 RTC 走时。这是设计规则，不是优化细节。
- `wifisetup.cpp` 里的 SSID 列表 UI 与 `main.cpp` 里的时区选择器是刻意互相重复的。项目规则是：共享抽象必须在去掉 M5GFX 依赖之后才允许抽取——`wifisetup.cpp` 里有对应注释。

---

## 10. 资源占用

干净构建实测（PlatformIO Core 6.1.19、espressif32 7.0.1、Arduino 核心 3.3.9、M5Unified 0.2.23、M5GFX 0.2.30）：

| 平台 | RAM | Flash |
|---|---|---|
| M5StickS3 | 52,872 B / 327,680 B（16.1%） | 1,176,559 B / 3,342,336 B（35.2%） |
| M5Cardputer | 53,480 B / 327,680 B（16.3%） | 1,176,451 B / 3,342,336 B（35.2%） |

sprite（64,800 B）占了 RAM 的大头，其余是 M5GFX/M5Unified 栈和 WiFi 驱动。应用分区还剩大约 2MB 空闲，所以真想做 OTA 是可行的。

---

## 11. 坑位索引

| 现象 / 任务 | 原因 | 处理位置 |
|---|---|---|
| 黑屏、不 panic、串口无输出 | sprite 分配失败 ⇒ 所有 canvas 绘制变成空操作 | `g_spriteOk` 兜底，§2 |
| `enum class` 成员叫 `HEX` 导致编译失败 | Arduino `Print.h` 的 `#define HEX 16` | 改名 `HEXADECIMAL`，§4 |
| SSID 显示乱码 | SSID 字段不是 NUL 结尾 | `strnlen` + 显式长度，§9 |
| 一次长按触发两个动作 | 短按/长按路径没有互斥 | v1.1.0 修复，§5 |
| 按住按键每秒重复 33 次 | 键盘每帧扫描且无边沿检测 | 26 位边沿掩码，§5 |
| 时钟从不进省电 | 省电以 `g_timeValid` 为门槛 | §3、§8 |
| USB 识别不到板子 | 充电线，或 CDC 端口被应用占用 | [BUILDING_CN.md](../BUILDING_CN.md) §7 |
| M5Burner 镜像被拒 / 刷完黑屏 | bootloader 用了 QIO 而不是 DIO | `board_build.flash_mode = dio`，[BUILDING_CN.md](../BUILDING_CN.md) §5 |
