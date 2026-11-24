# Changelog

## [1.0.0] - 2026-11-12

### Added
- Dedicated CST826 driver (renamed from local CST816S fork).

### Fixed
- Probe chip type at register `0xAA` (expect `0x11`); CST826 returns `0` at `0xA7`.
- Sleep uses register `0xA5` (CST826 family), not `0xE5` (CST816S).
- Wake from sleep via hardware RST.
- Accept touch coordinates at `(0,0)` on round panels.
- Soft-continue if ID probe fails without RST / before first touch.
