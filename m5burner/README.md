# M5Burner packaging · M5Burner 打包

**English** · [中文](#中文)

## English

`cyber-clock.json` is the M5Burner catalogue entry for this firmware — two
entries, one per device. A bundle the app can import consists of the `.json` plus
the merged images it names, zipped together:

```
cyber-clock.json                        ← catalogue entry (two devices)
cyber-clock-v1.1.0-merged.bin           ← merged flash image, StickS3
cyber-clock-cardputer-v1.1.0-merged.bin ← merged flash image, Cardputer
```

Import: M5Burner → **Custom** → **Import Custom FW** → pick the `.zip` (or the
`.json` alone) → select **Cyber Clock (StickS3)** or **Cyber Clock (Cardputer)** →
**Burn**.

The `.bin` must be a **merged** image written from offset `0x0`, with the
bootloader built in **DIO** mode. `tools/make-release.sh` produces exactly these
files and verifies the flash-mode byte (`0x02`); see
[BUILDING.md](../BUILDING.md) §5 for the manual esptool command.

The `category` field must match the device folder of the M5Burner version you are
publishing to (`M5StickS3`, `M5Cardputer`). If a future M5Burner renames a
category, that field — and nothing else — is what needs editing.

The images attached to the GitHub release are built from this repository, so the
`.bin` here and the release asset are byte-identical. Verify with the `sha256`
published in the release notes.

---

## 中文

`cyber-clock.json` 是本固件的 M5Burner 目录条目——两条，分别对应两台设备。可导入的打包内容就是这份 `.json` 加上它引用的合并镜像，压成一个 zip：

```
cyber-clock.json                        ← 目录条目（两台设备）
cyber-clock-v1.1.0-merged.bin           ← StickS3 合并镜像
cyber-clock-cardputer-v1.1.0-merged.bin ← Cardputer 合并镜像
```

导入：M5Burner → **Custom** → **Import Custom FW** → 选 `.zip`（或直接选 `.json`）→ 选中 **Cyber Clock (StickS3)** 或 **Cyber Clock (Cardputer)** → **Burn**。

`.bin` 必须是**从偏移 `0x0` 写入的合并镜像**，且 bootloader 是 **DIO** 模式。`tools/make-release.sh` 生成的就是这两个文件，并会校验 flash mode 字节（`0x02`）；手动 esptool 命令见 [BUILDING_CN.md](../BUILDING_CN.md) §5。

`category` 字段必须和你发布时所用的 M5Burner 版本里的设备目录名一致（`M5StickS3`、`M5Cardputer`）。如果将来的 M5Burner 改了目录名，需要改的就是这一个字段。

GitHub release 里挂的镜像就是从这个仓库构建的，所以这里的 `.bin` 与 release 附件逐字节一致。可用 release notes 里的 `sha256` 校验。
