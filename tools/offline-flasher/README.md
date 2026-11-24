# FPV Scout offline installer

Download the [latest offline installer](https://github.com/VALE-SOLUTIONS/FPV-Scout/releases/latest/download/FPV-Scout-Offline-Flasher.html)
and open it directly in desktop Chrome or Edge. It installs FPV Scout
on the RockBase NM-CYD-C5 with 16 MB of flash and contains all three release
images and their checksums. The page runs from disk without an internet
connection or server.

Use the native ESP32 USB-C port, then choose **Connect board**. The installer
enters download mode over USB Serial/JTAG and checks the chip, flash size and
security state before enabling backup and installation. The board label
identifies the target display hardware. If the USB reset fails, use **Manual
connection** with the board's **BOOT** and **RESET** buttons.

**Save 16 MB backup** reads and verifies the current flash and saves a local
file. Keep it private. **Install FPV Scout** writes and verifies the bootloader
at `0x2000`, the partition table at `0x8000`, and the app at `0x20000`. It writes
only the sectors covering those images. Press **RESET** after the app reports
that installation finished. See the [scanner recovery guide](../../docs/scanner.md#installation-and-recovery)
to restore a backup.

## Build the HTML

From the repository root, with Python and npm installed, fetch the pinned
build inputs once and generate the file. The builder verifies the tarballs and
release images against their SHA-256 checksums.

```powershell
New-Item -ItemType Directory -Force offline-packages
npm pack esptool-js@0.7.1 pako@2.1.0 atob-lite@2.0.0 spark-md5@3.0.2 --pack-destination offline-packages
python tools/offline-flasher/build.py offline-packages FPV-Scout-Offline-Flasher.html
```

With the tarballs present, the build also runs offline. It bundles the standard
release app at `firmware/fpv_scout.bin`. Pass `--app` with a path to a
checksummed standalone app to select a different build. The page includes the dependency licenses. The flasher
uses Espressif's [esptool-js](https://github.com/espressif/esptool-js/tree/v0.7.1)
and [Web Serial](https://developer.chrome.com/docs/capabilities/serial/).
