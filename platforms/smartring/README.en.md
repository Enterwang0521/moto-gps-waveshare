# Experimental SmartRing-Plus B-series port

> [中文](README.md)

Development baseline (2026-11-10): target **SmartRing-Plus B, not B V2**, as requested by the user. The build defines `MOTO_BOARD_SMARTRING_PLUS_B` and checks display, I2C and TF pins against the vendor BSP. This is a development target, not hardware revision detection. B V2-specific power features are outside the current scope.

TF uses SDMMC 4-bit: CLK=4, CMD=5, D0=3, D1=2, D2=7, D3=6, mounted at `/sdcard`. Planned routes and map directories are `/sdcard/routes` and `/sdcard/maps`. Preset-route support is not implemented yet; iPhone remains the positioning and navigation source.

This is a separate firmware target for the VIEWE SmartRing-Plus B series: ESP32-S3, 16 MB flash, 8 MB PSRAM, a 360 × 360 display and CST816 touch. It reuses the navigation protocol, iPhone app and UI; do not flash the Waveshare image to it.

The display and touch are rotated 180° so the screen is upright with the power connector at the bottom.

On one device, display, touch, iPhone BLE, route display and QQ Music / NetEase Cloud Music metadata and controls were tested. The exact B/B V2 revision remains unconfirmed. Gyroscope bias calibration and an approximately 90° tabletop rotation were tested; riding conditions, battery reporting, shutdown, power management and reconnects need further validation. Keep the device still for around eight seconds after startup; if calibration fails, it falls back to phone heading.

Initialize the LVGL submodule and activate ESP-IDF 5.5.5, then run from the repository root:

```sh
git submodule update --init --recursive
idf.py -C platforms/smartring build
```

Back up the specific device before its first flash. An app-only update is suitable only when its installed partition table matches this target. See the [Chinese build and hardware notes](README.md) for the code layout, font regeneration and detailed limitations. The firmware uses Apple Media Service when available and falls back to the iPhone app's system Music adapter otherwise.

An existing local `sdkconfig` overrides defaults. If linking reports undefined `ble_gattc_*` symbols, enable BLE Central and GATT Client in NimBLE's “Roles and Profiles” using `idf.py -C platforms/smartring menuconfig`. Fresh configurations enable both through `sdkconfig.defaults`.

On the music page, drag up or down along the left-hand arc to adjust volume in steps; touching it does not jump to a new volume. The indicator depends on the phone reporting volume. The outer green ring displays playback progress and cannot be dragged; progress and duration depend on the player reporting them. Start page-switching swipes in the middle of the screen.

For iPhone shortcuts, create the shortcuts in Apple's Shortcuts app, then enter their exact names for slots 1–3 in MOTO GPS under “圆屏快捷指令” and allow notifications. With the updated app connected, swipe to SHORTCUTS on the display and tap a number. In the foreground the app opens Shortcuts; in the background or on the lock screen, tap the notification and follow iOS unlock or confirmation prompts. Bluetooth and notification availability affect delivery. Automatic execution while locked is not guaranteed. This feature still needs device testing.

ESP-IDF's component manager may rewrite the LVGL path in `dependencies.lock` to a local absolute path when resolving dependencies. Review version changes and retain `../../third_party/lvgl` before committing the lock file.
