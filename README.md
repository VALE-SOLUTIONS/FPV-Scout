# FPV Scout - RockBase NM-CYD-C5

Standalone receive-only **FPV link scanning** on the RockBase NM-CYD-C5, with selectable portrait and landscape layouts, full-width analog video and a spectrum display.

<p align="center">
  <img src="docs/img/scout-menu.png" width="30%" alt="Scanning main menu with four signal cards and shortcuts to Spectrum, Settings, Signal Log and Composite Bench">
  <img src="docs/img/scout-waterfall.png" width="30%" alt="SCAN with waterfall above full-width video">
  <img src="docs/img/scout-spectrum.png" width="30%" alt="Spectrum page with the live sweep and waterfall">
</p>
<p align="center"><sub>Firmware-rendered previews with simulated spectrum and video data.</sub></p>

## Install

[Download the offline installer from the latest release](https://github.com/VALE-SOLUTIONS/FPV-Scout/releases/latest/download/FPV-Scout-Offline-Flasher.html). Open the HTML in desktop Chrome or Edge and connect the native USB-C port on the RockBase NM-CYD-C5. **Connect board** enters download mode automatically.

## Features

- **Scanning main menu** opens by default and cycles through analog video, digital video, control links and Remote ID. Tap a result to open its dedicated scanner.
- **Selectable startup page** lets you boot directly into the menu, a scanner, Spectrum, Signal Log or Settings.
- **Automatic video scanning** sweeps all 48 A/B/E/F/R/L presets, then compares up to three candidates by video sync and signal strength before locking.
- **Manual tuning** lets you step through presets, pick a band and channel, or enter a frequency from 5362 to 5945 MHz.
- **Rescan controls** let you start a new sweep, skip the current signal and nearby channels with **Continue**, or rescan automatically after five seconds of lost sync. Auto rescan can be turned off in Settings.
- **Analog channel recheck** keeps the last picture visible when link quality stays low. Scout checks the current channel against up to three of the strongest alternatives and switches when it confirms cleaner video.
- **Analog FPV preview** shows full-width grayscale PAL or NTSC video, including any readable on-screen text in the feed. The last picture stays visible through brief sync gaps.
- **Fullscreen video** hides the controls with one tap and keeps a transparent frequency, RSSI and LQ overlay. Tap again to return to the scanner.
- **Analog signal readouts** show the channel, frequency, video format, relative signal strength (RSSI) and valid-sync percentage (LQ).
- **Digital video alerts** flag wideband activity as **Digital signal** and show experimental **Likely DJI O4** or **Likely Avatar** matches when the timing fits.
- **Scan graph** switches between a live sweep and a waterfall when tapped. Both span 5.362–5.945 GHz, with 5.8 GHz centered and a marker at the tuned frequency.
- **Spectrum mode** shows a live trace, peak frequency and scrolling waterfall for 2.4 GHz or 5.8 GHz. **Hold** pauses the display.
- **Control Link Scanner** shows a 2.4 GHz waterfall and experimental ExpressLRS LoRa and DJI OcuSync alerts, with repeatability, RSSI and LQ readouts.
- **RemoteID Check** listens for Wi-Fi Beacon, Wi-Fi NAN and Bluetooth LE broadcasts. Recent reports show the broadcast ID when available, transport, signal strength and age.
- **Signal Calibration** saves separate on-board bench references for analog video, digital video, control links and Remote ID. [Step-by-step calibration guide](https://github.com/VALE-SOLUTIONS/FPV-Scout/wiki/Signal-Calibration)
- **Persistent Signal Log** keeps up to 32 entries across restarts. Tap for frequency, band, strength and first-seen, last-seen and signal-loss times. Save to microSD as CSV or clear the log.
- **Red signal LED** blinks from 1 to 30 Hz based on relative signal strength while a signal is locked. **Test LED** runs the 1, 5 and 30 Hz blink rates.
- **Portrait and landscape** layouts save your preference and keep separate touch calibrations.
- **Custom wallpapers** support PNG and animated GIF files on FAT32 or exFAT microSD cards. Pick an image on the CYD and choose **Main**, **All** or **Off**.
- **Composite Bench** previews selectable grayscale test patterns and generates wired PAL or NTSC test signals. [Composite Bench guide](docs/composite-bench.md)
- **USB diagnostics** report receiver status, scan timing, video sync, capture errors and wallpaper load results.
- **Offline USB installer** enters download mode, checks the chip and flash size, offers a verified full-flash backup and verifies the firmware write. [Installer guide](tools/offline-flasher)

## Use and build

- [FPV Scout Wiki](https://github.com/VALE-SOLUTIONS/FPV-Scout/wiki)
- [Offline flasher build source and instructions](tools/offline-flasher)
- [Installation, recovery, calibration and controls](docs/scanner.md)
- [Validation and remaining hardware checks](docs/validation.md)
- [Recovered code and video provenance](docs/reused-sources.md)
- [Manual flash images and offsets](firmware/README.md)
- [Build from source](docs/build.md)

The firmware source is in `espsoup/`. The tagged source and pinned build instructions correspond to the firmware bundled with the offline installer.

## Credits and license

The radio backend, FFT and capture engine derive from [ESPsoup](https://github.com/pit711/espsoup-firmware) by Peter Holzhauser and contributors, based on [ESPARGOS ESP-SDR](https://github.com/ESPARGOS/esp-sdr). Their GPL-3.0-or-later source notices, [LICENSE](espsoup/LICENSE) and [NOTICE](espsoup/NOTICE) are retained.

The LCD video renderer adapts the earlier NM-CYD-C5 port of **C5VRX by Twotoz and the C5VRX contributors**, based on upstream commit `8b60fef0f43f43a5305d941568c5975386867fc0`: [canonical source](https://github.com/Twotoz/C5VRX), [official website and Discord invite](https://twotoz.github.io/C5VRX/). It reuses snapshot FM discrimination, sync timing and grayscale line placement, with provenance and changes recorded in [recovered sources](docs/reused-sources.md). FPV Scout is an independent project. The combined firmware uses [GPL-3.0-only](LICENSE). See the original [C5VRX branding terms](licenses/C5VRX-branding.md) and [component notices](docs/reused-sources.md).

Inter font assets retain their [SIL Open Font License](tools/fonts/OFL.txt), also included beside the generated atlas.
