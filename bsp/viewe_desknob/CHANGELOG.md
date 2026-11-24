# Changelog

All notable changes to the VIEWE DeskNob BSP are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2026-11-12

### Changed
- Pull display/touch drivers from Component Registry: `viewesmart/esp_lcd_gc9b72` and `viewesmart/esp_lcd_touch_cst826` (no longer local `path:` overrides).

## [0.1.1] - 2026-11-12

### Fixed
- `bsp_display_new()`: use temporary handles and free SPI bus / panel on failure (no half-init “fake success”).
- `bsp_display_start_with_config()`: clear `s_disp` if LVGL adapter start fails; bind `max_transfer_sz` to draw buffer size.
- Touch I2C port: use the other port from `CONFIG_BSP_I2C_NUM` instead of hard-coding `I2C_NUM_1`.
- `BSP_CAPS_USB_MSC` now follows `CONFIG_BSP_USB_MSC`; USB APIs are stubbed when the feature is disabled.
- Button create: destroy already-created buttons if a later button fails.
- Audio I2S: delete channels on init failure.
- USB MSC: unmount wear-levelling / tear down SD MSC storage on storage or driver install failure.
- Touch: accept (0,0) on the round panel; demote per-touch logs to debug.
- Rename touch component to `esp_lcd_touch_cst826` (hardware is CST826, not CST816S).

### Changed
- Default LVGL stripe height `40` → `10` (align with Espressif SPI/QSPI BSP boards: `max_transfer = width × 2 × 10`).
- Default LVGL path uses DMA internal SRAM (`SPI_WITHOUT_PSRAM`) instead of PSRAM bounce buffers.
- `idf_component.yml`: add `url` and `license`; bump version to `0.1.1`.

### Added
- Root `LICENSE` (Apache-2.0) and this `CHANGELOG.md`.

## [0.1.0] - 2026-11-09

### Added
- Initial BSP for VIEWE DeskNob (ESP32-S31): display, touch, audio, SD, buttons, LED strip, battery, Wi-Fi/BT helpers, optional USB MSC.
