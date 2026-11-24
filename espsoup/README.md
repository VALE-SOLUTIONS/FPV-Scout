# FPV Scout source

This directory contains the current FPV Scout scanner for the RockBase NM-CYD-C5. The `espsoup` directory name is retained because the scanner reuses ESPsoup's receive-only RF backend.

See the [project README](../README.md), [scanner guide](../docs/scanner.md), and [validation record](../docs/validation.md). The original upstream description is preserved in [README-ESPsoup.md](README-ESPsoup.md).

## Build with ESP-IDF 6.0.2

Run from this directory after activating the SDK:

```powershell
idf.py -B build-scout -DIDF_TARGET=esp32c5 -DSDKCONFIG=build-scout/sdkconfig "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32c5;sdkconfig.defaults.cyd-c5" build
```

Flash the bootloader, partition table and app when replacing the old dual-app image. See [installation](../docs/scanner.md#installation-and-recovery); an app-only update leaves the old partition layout.

## Host test

From this directory with a host C compiler:

```sh
cc -std=c11 -O2 tests/scout/test_scout.c -lm -o test_scout
./test_scout
```

Keep assertions enabled: do not compile this test with `NDEBUG`.
