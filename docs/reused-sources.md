# Source and component notices

The combined FPV Scout firmware is distributed under [GPL-3.0-only](../LICENSE). The source tree retains the original license notices for individual components. The [build guide](build.md) identifies the ESP-IDF revision and release configuration.

| Component | Source and license |
|---|---|
| RF capture, FFT, tuning and spectrum | [ESPsoup](https://github.com/pit711/espsoup-firmware), by Peter Holzhauser and contributors, based on [ESP-SDR](https://github.com/ESPARGOS/esp-sdr). See the retained [license](../espsoup/LICENSE) and [notices](../espsoup/NOTICE). |
| Analog video receiver | **C5VRX by Twotoz and the C5VRX contributors**, [upstream source at the base revision](https://github.com/Twotoz/C5VRX/tree/8b60fef0f43f43a5305d941568c5975386867fc0), [official site and Discord invite](https://twotoz.github.io/C5VRX/). GPL-3.0-only. See the [original branding terms](../licenses/C5VRX-branding.md). |
| Composite Bench generator | Adapted from the C5VTX Composite Bench generator and test patterns, GPL-3.0-only. Timing, patterns and sample packing are in the current source tree. |
| ESP-DSP | Espressif's [ESP-DSP](https://github.com/espressif/esp-dsp), [Apache-2.0](../espsoup/components/esp-dsp/LICENSE). |
| ESP-IDF | Espressif's [ESP-IDF v6.0.2 and pinned submodules](https://github.com/espressif/esp-idf/tree/7101770dc6db2667b3c477cc31365dd1acd6db4e). See the [SDK license](../licenses/ESP-IDF-LICENSE.txt) and bundled binary Wi-Fi and PHY [library license](../licenses/Espressif-WiFi-PHY-LICENSE.txt). |
| Inter font | See [SIL Open Font License 1.1](../tools/fonts/OFL.txt). The generated glyph atlas carries the same license. |
| PNG and GIF decoding | [stb_image 2.34](https://github.com/nothings/stb/blob/1da9ade1e66195252114f5da497f244704a6c58d/stb_image.h), by Sean Barrett and contributors. The vendored header retains the upstream license. |
| Offline installer | [esptool-js and its dependencies](../tools/offline-flasher), with their licenses embedded in the offline HTML. |

The C5VRX-derived receiver adapts the earlier NM-CYD-C5 LCD port's FM discriminator, pulse detection, field reference and grayscale line drawing. FPV Scout uses finite 16,380-sample IQ10 snapshots at 20 MS/s, tracks field timing between captures, and keeps recent clean pixels when sync drops briefly. The port added the video pane, touch and orientation handling, spectrum integration and the fullscreen view.

FPV Scout is an independent project. The UI and documentation screenshots use the project's own interface and simulated signals.
