# Build FPV Scout

Use [ESP-IDF 6.0.2](https://github.com/espressif/esp-idf/tree/7101770dc6db2667b3c477cc31365dd1acd6db4e) at commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`, with its recursive submodules and ESP32-C5 tools installed. The 1.19.0 release uses Rust 1.100.0 and Espressif's RISC-V GCC 15.2 toolchain (`esp-15.2.0_20251204`). Put Cargo on PATH in the activated ESP-IDF shell. CMake builds the core offline from the committed lockfile.

Run from the repository root in PowerShell.

```powershell
rustup target add --toolchain 1.100.0 riscv32imac-unknown-none-elf
$env:RUSTUP_TOOLCHAIN = '1.100.0'
New-Item -ItemType Directory -Force build-scout
Copy-Item firmware/sdkconfig.scout build-scout/sdkconfig
$config = (Resolve-Path build-scout/sdkconfig).Path
$build = Split-Path $config
$defaults = 'sdkconfig.defaults;sdkconfig.defaults.esp32c5;sdkconfig.defaults.cyd-c5'
idf.py -C espsoup -B $build -DIDF_TARGET=esp32c5 "-DSDKCONFIG=$config" "-DSDKCONFIG_DEFAULTS=$defaults" -DSCOUT_VIDEO_PROBE=OFF build
```

The output contains `fpv_scout.bin`, `bootloader/bootloader.bin` and `partition_table/partition-table.bin`. Build timestamps and image metadata can differ between machines. See the [release flash layout and checksums](../firmware) and the [offline installer build](../tools/offline-flasher).

In an MSVC Developer Command Prompt with Cargo on PATH, run `tools\test_rust.cmd`. With a host C compiler available, also run `python tools/test_navigation_runtime.py` and `python tools/test_signal_log_storage.py`. Install the image-test dependency with `python -m pip install -r tools/requirements-fonts.txt` before running `python tools/test_wallpaper.py`. See [validation](validation.md) for the hardware checks.
