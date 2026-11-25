# FPV Scout hardware validation

## 1.19.0 scanning main menu and signal calibration

- The standard application was write-verified on the RockBase CYD. The
  release app and checked-in build output matched byte for byte. The stock
  installer partition table matched the verified layout. Bootloader differences
  in the build output were limited to build time and image integrity metadata.
- The CYD completed analog calibration on a live 5771 MHz NTSC bench feed.
  The bench check covered the menu's radio handoff and verified that sweeps resumed
  after exiting calibration with zero reported capture errors.
- A revised digital calibration check rejected the persistent analog bench
  feed from the quiet measurement. Positive digital, control link and
  Remote ID calibration results were exercised with host fixtures. Live
  positive calibration tests for those three tools remain open.
- Host tests cover saved calibration references, failed saves, quiet-baseline
  retries, selecting a startup page, scanner handoff, and keeping or changing
  the channel when analog LQ drops. The 1.18.1 bench runs completed forced
  analog rechecks in 0.46–0.75 seconds and kept the last picture visible.
- The release HTML opened in Chrome from disk and made zero HTTP requests.
  Simulated serial tests covered board checks, backup, write verification and
  recovery after a write failure.

## 1.17.0 persistent Signal Log and fullscreen

- A live analog entry survived a CYD restart. The board exported the entry to
  `SC000001.CSV` on its microSD card, then showed an empty active log.
- Fullscreen entry and exit were exercised on the CYD with all 118 analog
  source rows filled. Host tests cover touch navigation, the transparent
  overlay, portrait and landscape drawing, and a second tap returning to SCAN.
- Storage tests cover log restore, first/last/lost times, failed exports,
  filename collisions, and preserving entries when clearing fails.
- The final standard app was hash-verified and held all 118 video rows with
  zero capture errors during the post-flash check. The offline installer
  passed in Chrome with networking disabled.

## 1.16.0 Signal Log

- The CYD received the live analog bench feed and showed one history entry
  after repeated observations. Auto had locked to the overlapping B3 preset.
- The final image was hash-verified and booted into Auto. It locked to R4 in
  743 ms on the first sweep and held a full picture during the check.
- Host tests cover paging in both orientations, duplicate handling, eviction
  at 32 entries, and input from video, control link, and Remote ID detections.
  Previews use simulated data. The offline installer passed in Chrome with
  networking disabled.

## 1.15.5 video decode speed

On the static R4 bench feed, average decode time fell from 5.36 to 4.11 ms per
capture. The receiver processed about 117 RF snapshots per second, up from 98,
and wrote about 626 decoded rows per second, up from 516. The display continued
to update about 56 times per second.

Replaying six saved RF captures through both decoders produced byte-for-byte
identical display output across 757 updates. Host tests also check that noise
outside the nominal line span is still masked when alignment shifts the row.
All 30 live picture samples contained fresh data and all 118 rows. The final
app was hash-verified on the CYD and locked to R4 on the first sweep in 742 ms.
The offline installer passed the Chrome test with networking disabled.

## 1.15.4 video update speed

Matched landscape bench builds on the same static R4 feed measured 13.9 and
54.5 LCD updates per second for versions 1.15.3 and 1.15.4, respectively.
Average display time per update fell from 21.6 to 1.86 ms, and the receiver
processed about 99 RF snapshots per second, up from 74. Each display update
can include only a subset of the video rows.

All 34 display samples from the high-refresh build had 118 of 118 rows and
fresh picture data. The offline installer passed in Chrome with networking
disabled. The final hash-verified app booted into Auto, locked to R4 758 ms
into its second sweep, and kept all 118 rows filled during the check.

Host tests cover changed-row drawing, partial preview interpolation and the
simulated LCD during a scene change. Tests also cover PAL and NTSC in both
orientations and the Auto candidate comparison.

## 1.15.3 analog picture stability

- Across the staged builds, the live R4 test pattern showed less horizontal
  jitter and fewer streaks. In the 50-second full-field-clock check, 101 display
  samples contained fresh rows. Median frame-to-frame change fell from 6.80
  to 3.03 on a 0–255 grayscale scale. Rows with an average absolute deviation
  of 20 or more from the baseline median image fell from 1.45 to 0.14 per sample.
- Replaying the same R4 capture through both decoders lowered the 99th-percentile
  row error from 27.41 to 7.26. Nearby presets also improved in replay.
- After the preview threshold change, the first sampled pictures on two live
  starts had 117 and 113 of 118 rows. Both reached full coverage and all 61
  display samples contained fresh rows.
- The final app was hash-verified on the CYD and booted into Auto. It locked
  to R4 on the second sweep, 760 ms into that sweep, and held all 118 rows.
- Tests cover PAL and NTSC in both orientations and polarities, sync-edge
  correction, startup coverage, and a synthetic scene change that refreshed
  at least 90% of rows in about 230 ms. The live bench used a static picture.
- The offline installer passed the Chrome test with networking disabled.

## 1.15.2 overlapping analog presets

- With one live R4 transmitter, eight Auto runs locked with 116–118 of 118
  landscape rows available at the post-lock check. Five chose R4, two chose
  nearby B3, and one chose nearby A6. The successful sweeps reached lock in
  739–756 ms from the start of those sweeps. One run needed a second sweep.
- Host tests verify that a nearby confirmed contender with enough clean rows
  can win. A small picture difference, too few samples, and a stronger picture
  score at a distant preset leave the original sync result in place.
- On manual 5769 MHz, all 28 display samples across 14 seconds contained fresh
  rows. The test text was readable, with occasional horizontal streaks.
- The standard app was hash-verified on the CYD. It booted directly into Auto,
  locked in 749 ms, and kept all 118 landscape rows available during the check.
- The offline installer passed the Chrome test with networking disabled.

## 1.15.1 analog video and Remote ID

- The CYD's video readbacks showed fewer horizontal artifacts on an R4 test pattern.
  On identical recorded IQ, the 95th-percentile line alignment error fell from
  0.23 line to 0.02 line. The final app was hash-verified and kept all 118
  landscape rows updating throughout a 21-second Auto check on nearby B3.
- Host tests exercise PAL and NTSC, integer-line progressive fields, both
  orientations and modulation polarities, and response to a scene change.
  Live picture quality still depends on reception and source sync.
- The CYD decoded a live Bluetooth Remote ID broadcast and recovered its analog
  video after leaving **RemoteID Check**. Live Wi-Fi management reception and
  Wi-Fi Remote ID packet decoding were tested separately.
- The offline installer passed the Chrome test with networking disabled.

## Early hardware and host checks

Updated 2026-11-21 UTC for the RockBase NM-CYD-C5.

| Check | Result | Evidence and limits |
|---|---|---|
| ESP-IDF 6.0.2 / ESP32-C5 build | Passed | App 1,010,576 bytes (`0xf6b90`), within the 2 MiB factory partition; bootloader 22,304 bytes (`0x5720`) |
| Synthetic PAL and NTSC FM IQ | Passed | Same C detector as firmware; both modulation polarities, with and without added IQ noise (amplitude 12 against carrier amplitude 150) |
| Noise and carrier rejection | Passed for synthetic inputs | 500 random-IQ snapshots and 500 noisy unmodulated-carrier snapshots; these do not establish a real-world false-alarm rate |
| Horizontal sync without vertical sync | Rejected | H-only input cannot trigger VIDEO FOUND |
| Mixed-polarity evidence | Rejected | Opposite polarities cannot combine into a confirmed hit |
| Candidate deadline | Passed | Timed-out evidence is cleared |
| Lock/resume state logic | Passed on host | Lock survives missing sync; resume clears evidence |
| Full pre-install backup | Verified | 16 MiB read and matching board digest; private backup retained locally, not included in this repository |
| Board installation | Verified writes | Bootloader, partition table and app each reported a matching flash hash |
| Display startup | Confirmed by the operator | The 1.3 portrait spectrum-above-video layout is running after touch calibration |
| C text renderer and screen layout | Passed on host | Six actual C-rendered states; proportional widths, antialiasing, negative/edge clipping and bounded writes |
| 1.1 UI installation | Hash verified | App at `0x20000`; previous 1.0 app saved and verified; NVS and partition map retained |
| 1.2 spectrum/video installation | Hash verified | App at `0x20000`; previous 1.1 app saved and verified; NVS and partition map retained |
| USB read-only diagnostics | No response | `SCOUT?` returned no usable telemetry during the session |
| Real analog VTX detection | Pending | No transmitter was available |
| Physical Lock/Continue controls | Pending | Host state tests do not verify touch input or on-board behavior |
| Weak signals, Wi-Fi rejection, range and sweep timing | Pending | No independent hardware measurements |

Windows reported a USB device-disappearance error during the post-flash watchdog reset. This occurred after all three write hashes were verified. The operator subsequently confirmed that the scanner was running on the display.

## Next hardware checks

1. With no VTX on, observe several full passes and confirm that noise/Wi-Fi does not cause a VIDEO FOUND alert.
2. Power a known analog PAL or NTSC VTX at a known listed frequency and confirm detection and the displayed channel/frequency. Try both standards when available.
3. Tap LOCK, remove the signal and confirm that the board remains on frequency and reports sync loss. Restore the signal and check recovery.
4. Tap CONTINUE and confirm scanning resumes without immediately stopping on an alias of the same carrier. Repeat with BOOT controls.
5. Measure full-pass time, detection delay, weak-signal behavior and false alerts before making performance claims.

Do not infer real-transmitter compatibility or range from the synthetic tests or simulated UI previews. The detector recognizes analog composite sync; it cannot distinguish a live scene from a recording or static video source.

The 1.1 change updates typography and drawing only. The RF detector and scan/lock state headers are unchanged from the tested 1.0 version. Physical signal detection and button-operation checks remain pending.

## 1.2 spectrum/video update

- Firmware build: passed with ESP-IDF 6.0.2; app 1,008,112 bytes, within the 2 MiB factory partition. The SDK also emitted a missing ROM-ELF debug-helper warning; firmware generation and size checks completed.
- Actual C UI rendering: all six states pass clipping/bounds checks with the spectrum above the video pane. Preview video bars are simulated.
- Adapted video decoder: synthetic PAL/NTSC IQ, both polarities, and separated snapshots produce bounded grayscale rows; standard detection and the scratch-bank guard pass. This is host-model evidence only.
- The tests exposed and corrected skipped vertical-only intervals and a polarity-selection problem in the earlier LCD prototype.
- The prior 1.1 app was preserved and matched against the board before the update. The 1.2 app was written at 0x20000 and its flash hash verified.
- Real-transmitter picture quality, sync retention and scan performance remain unverified.

The operator confirmed that the 1.2 board layout shows the spectrum above the video pane. No transmitter was available to verify a live received picture.

## Portrait update 1.3 - 2026-11-21

- ESP-IDF build passed; app size 1,007,968 bytes, within the 2 MiB factory partition.
- Actual C previews of all six states pass bounds, proportional-font and edge-clipping checks at 240x320.
- Synthetic PAL/NTSC video tests pass with the larger 192x144 viewport in both polarities across snapshot gaps.
- Previous 1.2 landscape app saved and verified against the board before updating. Portrait app write hash verified.
- Operator confirmed portrait operation after completing the three touch-calibration targets.
- Spectrum and video remain vertically stacked; live received video, physical signal locking and RF performance remain unverified without a transmitter.

## Full-width video and fixed spectrum 1.4 - 2026-11-21

- Build passed; app 1,009,296 bytes within the 2 MiB partition.
- All six actual C UI states pass bounds/clipping checks with a 240x180 edge-to-edge video pane and streaming text overlays.
- Synthetic PAL/NTSC video tests pass in both polarities with overlays linked into emitted rows.
- Band tests cover all 216 columns from 20 chunks, exact endpoints/midpoint, monotonic mapping, bounds and the placement of a simulated 5.8 GHz peak.
- Tests of the actual sweep helper verify that the receive frequency/mode/filter are restored on successful and failed captures, with missing-data handling.
- Previous 1.3 app saved and verified; 1.4 app flash hash verified. Operator confirmed the updated full-width layout and fixed labels.
- Actual sweep refresh time, real-transmitter picture quality under RF time-sharing, and physical signal locking remain unverified.

## Main menu 1.5 - 2026-11-21

- Firmware build passed; app 1,010,576 bytes within the 2 MiB partition.
- Eight actual C-rendered screens pass layout/font/bounds checks: six receiver states, main menu and Settings.
- Navigation hit-area tests pass for SCAN/SETTINGS cards, both Back buttons, calibration, existing scan controls, gaps and out-of-bounds input.
- Runtime tests of the actual input/UI-step code verify boot-to-menu, no UI RF capture on menu/settings pages, held-touch debounce, calibration dispatch, and cleared detection evidence on return to scanning.
- Previous 1.4 app preserved and verified against the board; 1.5 app write hash verified.
- Operator confirmed opening SCAN, returning with Back, and opening SETTINGS on the board. Settings-triggered recalibration was not physically rerun during this update; it reuses the existing portrait-calibration routine.
- Real-transmitter tests remain pending.

## Receive-only red LED update: Rust 0.2 / source 1.6 (2026-11-21)

ESP-IDF 6.0.2 build passes: app 1,024,752 bytes, 51% of the factory app partition
free. Its write at 0x20000 was hash-verified. The installed Rust 0.1 app was
verified before replacement and preserved; the prior C 1.5 app is also retained.

Synthetic Rust detector/navigation/spectrum tests still pass. New checks cover
the LED scale's monotonic 1–30 Hz bounds, inactive/stale gating, actual WS2812
bitstream/reset timing through mocked RMT/timer APIs, nonblocking retry, test
expiry, Settings routing and LED shutdown on sync loss/capture failure/Back.
Actual C screen rendering and RF-setting restoration checks pass.

Physical LED test confirmation is pending. No transmitter is available: no real
RF-strength correlation, received picture or signal-lock performance is claimed.
The metric is relative spectral contrast, not calibrated RSSI or transmitter watts.
No transmit or VTX-control path is included.

## Composite Bench in Rust 0.4

- ESP-IDF 6.0.2 built the 1,039,216-byte application. The linker map confirms that Composite Bench mode and preview sampling resolve to the Rust archive.
- Host tests pass for both timing families, both DAC packings, black startup, porch and sync placement, grayscale quantization, animation, level limits and buffer bounds.
- The Rust device-mode lifecycle passes with mocked ESP-IDF calls, including PAL restart, startup failure cleanup and retry after a failed shutdown. The app-loop tests cover receiver pause, local-only bench activation, command isolation and scan resumption.
- The installed Rust 0.3 app and partition table matched the rollback image and partition file before the update. The Rust 0.4 write at `0x20000` passed hash verification.
- External monitor lock, analog levels at P5 and real FPV reception remain unverified.

## Selectable display orientation in Rust 0.5

- Host rendering checks cover both panel sizes, all scan states, both menu pages and Composite Bench. Synthetic video continues to draw bounded PAL and NTSC rows in both orientations and polarities.
- Rust navigation tests cover the Orientation button and both layouts. UI runtime tests verify that switching stays on Settings, loads the orientation's calibration and keeps receiver capture paused.
- The installed Rust 0.4 app and partition table matched before flashing. The Rust 0.5 app write at `0x20000` passed hash verification.
- The operator confirmed the CYD switches to landscape and that the touch controls work.

## SD wallpaper in Rust 0.6

- The device decoder passes host tests for PNG transparency, both display orientations, GIF frame delays and looping, and fallback for missing or malformed images. The UI runtime test confirms animation advances only on the main menu.
- The ESP32-C5 build keeps PSRAM allocations separate from internal DMA memory. Its linker map verifies that internal static sections stay below the RF dump bank and the scratch space fits within the reserved spectrum region.
- The installed Rust 0.5 app and partition table matched before flashing. The Rust 0.6 app write at `0x20000` passed hash verification.

## exFAT wallpaper fix in Rust 0.7

- On the CYD, probing the inserted card succeeded but FatFs returned `FR_NO_FILESYSTEM` at both attempted SPI speeds. The ESP-IDF FatFs configuration had exFAT disabled.
- With exFAT support enabled, the same card loaded its 96 × 128 GIF at startup and on USB reload. The operator confirmed the wallpaper was visible and animating on the CYD. The app-only write passed hash verification.

## Wallpaper modes in Rust 0.8

- Host checks verify the default and saved preference handling, both settings layouts, wallpaper on all four pages when enabled, and that redraws preserve live scan video rows and the Composite Bench preview.
- The Rust 0.7 app and partition table matched before the update. The app-only Rust 0.8 write passed hash verification.
- The operator confirmed that all three modes work and reported flickering controls in Settings and SCAN with ALL selected.

## Wallpaper redraw in Rust 0.9

- Host LCD tests exercise the display driver's actual fill and blit code in both orientations. During animated redraws, the sampled settings and scan buttons never receive the wallpaper color. The driver sends no wallpaper transfers to the live scan video rows.
- The Rust 0.8 app and partition table matched before the update. The app-only Rust 0.9 write passed hash verification.
- The operator confirmed Settings and SCAN stay steady with the animated GIF and ALL selected.

## Manual channels and scan speeds in Rust 0.10

- Host tests exercise the actual scan loop and touch actions. Manual holds the selected channel through capture errors and verification timeouts. Next channel releases a manual hold and steps exactly once, including at channel wrap.
- The four Auto presets vary scheduling and spectrum refresh. Tests assert that scan attempts still use the same capture size and rate and that Auto detection retains Lock and Continue.
- The Rust 0.9 app and partition table matched before the update. The app-only Rust 0.10 write passed hash verification.

## Scan action labels and rate limits in Rust 0.11

- The left button shows Manual during automatic searching and Auto scan while monitoring a manually selected channel.
- Host tests verify automatic intervals of 200, 100, 50 and 33.334 ms, preserving the current channel and updating the UI and spectrum while waiting. Pacing does not delay manual monitoring, confirmation or video holds.
- The Rust 0.10 app and partition table matched before the update. The app-only Rust 0.11 write passed hash verification.

## Strongest-signal sweep and preview in Rust 0.12

- A host sweep confirms B3 first and then a stronger B5. It visits all 48 entries before selecting B5 (5809 MHz). Separate signal-score tests verify that DC is ignored, a centered peak outranks an off-center alias in the fixture, and a genuinely stronger centered candidate can win.
- Synthetic PAL/NTSC checks cover both polarities and display orientations. Every preview redraw covers the entire pane, including when captured rows are still sparse. Tests confirm the held-channel video capture still runs between periodic detector checks.
- The Rust 0.11 rollback app and partition table matched before the app-only Rust 0.12 update. The new write passed hash verification.
- With a powered transmitter, the operator confirmed lock on the active channel. The preview flashed a mostly complete picture about every other second, and transmitted text remained unreadable.

## Video sync and retained preview in Rust 0.13

- The preview uses 20 MS/s IQ10 snapshots and the scan detector's six-bit phase quantisation. Spectrum retunes are half as frequent during a hold.
- Host tests check PAL/NTSC in either modulation polarity and orientation, confirm that two different luminance bands land in the correct parts of the pane, and verify that a sync gap does not blank the last picture prematurely.
- The installed Rust 0.12 app and partition table matched before the app-only update. The Rust 0.13 flash passed hash verification and SCOUT? confirmed the new version booted.
- During a 25-second live sample, the CYD held F4 (5800 MHz) and retained all 180 portrait rows with zero capture errors. Horizontal sync stayed present in sampled telemetry, and vertical sync dropped and recovered without clearing the row cache. The operator reported a mostly steady picture and readable transmitted text, with some remaining motion or unevenness. The operator subsequently confirmed the transmitter was set to F4.

## Field timing in Rust 0.14

- A five-second capture on F4 returned 91 checksum-verified IQ10 windows. Replaying those windows through the 0.13 display decoder reproduced vertical movement and visible static. The received NTSC field interval was about 16,663 µs, roughly 21 µs shorter than the decoder's nominal interval.
- Replaying the same windows with measured field timing reduced movement of the high-contrast lower edge under the transmitted text from five portrait pixels to no movement in the sampled frames after the text appeared. Some static remains. USB capture windows are more widely spaced than during on-device viewing.
- The host video test includes the measured NTSC clock offset and checks that PAL and NTSC recover the expected line period with either modulation polarity and display orientation.
- The Rust 0.14 app write passed hash verification. After reboot the CYD locked to F4 in Auto and reported all 180 portrait rows, both sync flags, and zero capture errors.
- The operator saw about the same amount of vertical movement on the CYD after the timing update. The replay improvement did not translate into a noticeable on-screen change.

## Direct scan startup in Rust 0.15

- The host navigation test calls the real UI initializer, confirms Auto at 30x, runs one RF check and exercises Back to the main menu. Further UI steps on the menu leave the capture count unchanged.
- After the app update on the CYD, USB telemetry showed the receiver already moving through channels in Auto at 30x with no capture errors.

## Auto rescan in Rust 0.16

- The navigation runtime test covers a five-second timeout, renewed sync restarting the timer, the OFF selection surviving a simulated reboot, and a Manual lock returning to Auto when the toggle is on.
- Navigation tests cover the toggle in portrait and landscape. The Settings render and wallpaper redraw checks passed in both orientations.
- The Rust 0.16 app write passed hash verification. The CYD booted into automatic scanning and reported the default Auto rescan setting as ON over USB.

## Fast sweep and candidate comparison in Rust 0.17

- The host runtime test visits all 48 entries in Fast mode and compares the RF-strength shortlist in two rounds. A cleaner candidate wins over a stronger candidate that also qualifies, while an H-only candidate fails confirmation. The four paced presets also pass.
- The Rust 0.17 app write passed hash verification. In one live Fast-mode run on the CYD, the full sweep took 299 ms and three-contender comparison took 301 ms. The receiver locked to F5 (5820 MHz) in 718 ms from the first channel check and reported zero capture errors.
- The hardware run establishes timing for that setup. The competing signal quality fixture used simulated evidence.

## Offline flasher

- Chrome loaded the standalone HTML with networking disabled. The browser tests exercised board checks, backup download, write progress, and error recovery with a simulated serial device.
- The bundled loader put the running CYD into download mode over native USB, read a 1 MiB flash chunk, and matched the device's checksum. The operator confirmed that automatic connection worked from the browser.

## Landscape rotation in 1.12.6

- The display and touch test checks the 180-degree landscape rotation, retained calibration, and recalibration targets in both orientations.
- The app-only flash passed hash verification. The CYD restarted into 1.12.6 and reported its saved Auto rescan setting.

## SCAN and Spectrum in 1.12.7

- On the CYD a 48-channel pass took 321 ms with zero capture errors. The host test exercised the full sweep and the unchanged top-three comparison, including a case in which the cleanest signal beat a stronger one.
- The operator confirmed the SCAN graph toggle, the live Spectrum trace, and cycling images on the microSD card. After reboot, USB status showed the selected wallpaper loaded.
- Display-transfer checks in portrait and landscape verify that live plot updates skip the controls and that the SCAN graph toggle leaves the video region untouched. The operator confirmed that both pages stay steady.
- Chrome loaded the offline installer without network requests. The manifest and simulated serial tests passed.
