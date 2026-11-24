# Firmware images

The standard release is **FPV Scout 1.17.0**, built for the RockBase NM-CYD-C5
with ESP32-C5, 16 MB flash and 8 MB PSRAM. The
[latest offline installer](https://github.com/VALE-SOLUTIONS/FPV-Scout/releases/latest/download/FPV-Scout-Offline-Flasher.html)
bundles all three images.

| File | Flash address |
|---|---|
| `bootloader/bootloader.bin` | `0x2000` |
| `partition_table/partition-table.bin` | `0x8000` |
| `fpv_scout.bin` | `0x20000` |

For manual installation on a stock board, write all three images.
On a board with the verified standalone Scout partition map, write only
the app at `0x20000` for a firmware update. `flasher_args.json` lists the
offsets and flash settings. `SHA256SUMS.txt` covers the three binaries.

See the [scanner guide](../docs/scanner.md#installation-and-recovery) for backup,
download mode, and recovery.

See the [build guide](../docs/build.md) for the source and exact release configuration.
