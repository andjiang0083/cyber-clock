# CYBER CLOCK 构建与刷机

**中文** · [English](BUILDING.md)

从 `git clone` 到桌面上跑起来所需的全部步骤，外加我们真踩过的坑。只想刷现成镜像的话，[README_CN.md](README_CN.md#快速开始) 里有两行命令的版本，本文不看也行。

---

## 1. 准备工作

| | |
|---|---|
| 板卡 | M5Stack **StickS3** 或 M5Stack **Cardputer**（都是 ESP32-S3） |
| 线材 | USB-C **数据线**——只有充电功能的线是"识别不到板子"的头号原因 |
| 主机 | macOS / Linux / Windows |
| Python | 3.8+ |
| PlatformIO | Core 6.x（CLI 或 VS Code 插件） |

没装 PlatformIO Core 的话：

```bash
python3 -m pip install -U platformio     # 或: brew install platformio
pio --version                            # PlatformIO Core, version 6.x
```

驱动：StickS3 和 Cardputer 用的都是 ESP32-S3 的**原生 USB-Serial/JTAG** 外设，macOS 和 Linux 不需要装任何驱动，Windows 10/11 一般也能自动识别。如果 Windows 报未知设备，装乐鑫的 USB-JTAG 驱动即可——但要注意固件是用 `ARDUINO_USB_CDC_ON_BOOT=1` 编译的，**应用启动后串口会消失再重新出现**。这是正常现象，不是崩溃。

---

## 2. 取源码

```bash
git clone https://github.com/andjiang0083/cyber-clock.git
cd cyber-clock
```

仓库里内嵌了固件依赖的 GUI 框架（`arcadia/core/`，通过 `#include <core/arcadia.h>` 引用）。**不要删这个目录**——删了立刻报 `fatal error: core/arcadia.h: No such file or directory`。

---

## 3. 构建

### StickS3 — 在仓库根目录

```bash
pio run
```

### Cardputer — 在它自己的工程目录里

```bash
cd cardputer
pio run
```

这是两个独立的 PlatformIO 工程，各自有独立的 `.pio/` 构建目录。两个目标本质上是同一套代码，但各自在自己的 `src/` 下有一份拷贝——改共享行为时**两边都要改**（`src/` 和 `cardputer/src/`）。这是代码库里唯一一处重复，[CONTRIBUTING_CN.md](CONTRIBUTING_CN.md) 里也专门写了。

产物在 `.pio/build/<env>/`：

| 文件 | 用途 |
|---|---|
| `firmware.bin` | 只有应用——配合 `pio run -t upload` |
| `firmware.factory.bin` | PlatformIO 自己合并的镜像（bootloader + 分区表 + 应用） |
| `bootloader.bin`、`partitions.bin` | 想自己手动合并时用 |

---

## 4. 刷机

```bash
# macOS            Linux              Windows
# /dev/cu.usbmodem*  /dev/ttyACM*     COMx
pio run --target upload --upload-port /dev/cu.usbmodem1101
```

用 `pio device list` 找端口。如果上传卡在 `Connecting...`，在上传重试期间按住板子的复位键，或者拔插一次 USB。

串口日志（115200，`platformio.ini` 里已打开 ESP32 异常解码器）：

```bash
pio device monitor
```

> 串口是应用的，不是调试控制台：固件不会周期性打印任何东西，所以监控窗口空着才是常态。屏幕上也没有调试信息——这是刻意的。

---

## 5. M5Burner / esptool 用的合并镜像

M5Burner 要的是一个**从偏移 `0x0` 写入的合并镜像**，而且 bootloader 必须是 **DIO** flash mode（`platformio.ini` 里已经设好：`board_build.flash_mode = dio`）。可以直接用第 3 步产出的 `firmware.factory.bin`，也可以显式合并——`tools/make-release.sh` 做的就是这件事：

```bash
./tools/make-release.sh            # 两个版本的合并镜像都会输出到 dist/
```

手动合并 StickS3：

```bash
ESPTOOL=~/.platformio/packages/tool-esptoolpy/esptool.py
python3 $ESPTOOL --chip esp32s3 merge_bin \
  --output dist/cyber-clock-v1.1.0-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0000 .pio/build/sticks3/bootloader.bin \
  0x8000 .pio/build/sticks3/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/sticks3/firmware.bin
```

Cardputer 版（在 `cardputer/` 目录下执行）：

```bash
python3 $ESPTOOL --chip esp32s3 merge_bin \
  --output ../dist/cyber-clock-cardputer-v1.1.0-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0000 .pio/build/cardputer/bootloader.bin \
  0x8000 .pio/build/cardputer/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/cardputer/firmware.bin
```

### 发布/刷机前先验证镜像

```bash
# 1. bootloader 头里的 flash mode 字节必须是 0x02 (DIO)
python3 -c "d=open('dist/cyber-clock-v1.1.0-merged.bin','rb').read(); print('DIO' if d[2]==0x02 else hex(d[2]))"

# 2. 校验和
shasum -a 256 dist/cyber-clock-v1.1.0-merged.bin
```

用 QIO（`0x00`）编出来的合并镜像用 esptool 刷没问题，但 M5Burner 会拒绝或者刷完起不来。如果 M5Burner 刷完是黑屏，第一个要查的就是这个字节。

---

## 6. 可复现性

发布镜像由本仓库用以下版本构建：

| 组件 | 版本 |
|---|---|
| PlatformIO Core | 6.1.19 |
| `platform = espressif32`（实际解析） | 7.0.1 |
| Arduino core for ESP32 | 3.3.9 |
| M5Unified | 0.2.23 |
| M5GFX | 0.2.30 |
| esptool | 5.3.0 |

`platformio.ini` **没有**锁库版本（`m5stack/M5Unified` 没写版本约束），所以今天全新 clone 可能拉到更新的 M5Unified，编出来的镜像会大几 KB。这是预期内的：源码一样，链进去的库不一样。锁定平台与库版本是一项待办的 [roadmap](ROADMAP_CN.md) 事项——欢迎提一个把版本钉死、并把实测尺寸记录下来的 PR。

在作者机器上实测（PlatformIO 包缓存已存在）：

| 步骤 | 耗时 |
|---|---|
| StickS3 干净构建（`rm -rf .pio && pio run`） | 145 秒 |
| 紧接着构建 Cardputer（包已缓存） | 22 秒 |

全新机器上第一次构建会更慢——PlatformIO 要先下载平台、工具链和 Arduino 框架，之后才开始编译。

---

## 7. 故障对照表

| 现象 | 原因 | 处理 |
|---|---|---|
| `fatal error: core/arcadia.h: No such file or directory` | `arcadia/` 缺失或被移动 | 恢复该目录；包含路径是 `-I${PROJECT_DIR}/arcadia`（StickS3）/ `-I${PROJECT_DIR}/../arcadia`（Cardputer） |
| Cardputer 编进了 StickS3 的头文件 | 构建目录搞错了 | Cardputer 工程必须在 `cardputer/` 里构建，不能在根目录 |
| `pio device list` 什么都没有 | 充电线，或端口已被应用占用 | 换数据线；因为 `USB_CDC_ON_BOOT`，启动后端口会消失再出现 |
| 上传卡在 `Connecting...` | 板子正在 Light Sleep / 应用占着 | 上传开始时按住复位键 |
| 刷合并镜像后黑屏 | QIO bootloader，或下载不完整 | 检查第 2 字节是否为 `0x02`，重新下载并核对 `sha256` |
| 正常上传后黑屏，但复位能看到开机动画 | Sprite 分配失败 | 固件会退化为直接渲染；若持续出现，请带目标版本和复现步骤开 issue |
| M5Burner 拒绝文件 | 不是合并镜像，或不是从 `0x0` 写 | 用第 5 步的合并镜像 |
| 时钟显示开机计时 | NTP 从未成功 | 检查 2.4GHz 网络；重新配网（开机动画期间按住 `BtnA` / Cardputer 按 `W`） |
| 中文 SSID 显示成 `□` | 字体和键盘只支持 ASCII | 已知限制，见 [README_CN.md](README_CN.md#已知限制) |
| 时钟卡死，必须断电才能恢复 | 疑似硬件级死锁，不是 panic | 报 issue 时务必写明目标版本——这正是 issue 表单要问的那类问题 |

---

## 8. 想改点什么

| 改什么 | 在哪 |
|---|---|
| 开机动画上的版本号 | `src/config.h` 与 `cardputer/src/config.h` 的 `VERSION` |
| 闲置阈值（60s / 180s） | 两个 `config.h` 的 `IDLE_POWERSAVE_MS`、`IDLE_SLEEP_MS` |
| 亮度档位 | 两个 `config.h` 的 `BRIGHTNESS_LEVELS` / `BRIGHTNESS_PCT` |
| 雨滴数量、字符池、拖尾长度 | `main.cpp` / `config.h` 的 `MATRIX_DROPS`、`MATRIX_CHARS`、`TRAIL_LEN` |
| 配色 | 两个 `main.cpp` 的 `initThemes()` |
| 帧率上限 | `loop()` 末尾的 `delay(30)` |

改完之后，**两个目标都要重新构建**再算完成。
