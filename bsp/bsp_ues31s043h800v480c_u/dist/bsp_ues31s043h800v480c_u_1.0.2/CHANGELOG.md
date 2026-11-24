# Changelog / 版本变更

## 1.0.2 - 2026-11-07

按原理图 `SCH_UES31S043H800V480C-U` V1.1 修正音频时钟、功放增益和 SD 卡电源。ADC 按键窗口未改。

- I2S MCLK 接到 `GPIO48`。ES8389 的 `use_mclk` 保持关闭：当前 `esp_codec_dev` 只在关闭时写入分频表，时钟取自 BCLK。打开后分频停在初始值，DAC 无声。SCLK / LRCK / DOUT / DIN 仍是 GPIO49–52。
- NS4150B 输入电阻 100 kΩ，`pa_gain` 设为 9.5 dB。供电补偿仍是 5.0 V / 3.3 V。同一音量值会比 1.0.1 低约 9.5 dB。
- 初始版开发板的 SD 电源常开，不由 GPIO60 或 GPIO57 控制。原理图 V1.1 的 `SD_CTRL` 才是 GPIO57（低电平保持上电，拉高关闭 AO3401A）。卸载成功后拉高 GPIO57，并清掉卡句柄。
- PCLK 从 25 MHz 降到 22 MHz。消隐仍是水平/垂直 4/8/8。刷新约 54 Hz：22 MHz / ((800 + 4 + 8 + 8) × (480 + 4 + 8 + 8))。

## 1.0.1 - 2026-10-19

RGB 时序调整。Bounce 仍为 20 行，帧缓冲仍为 2 块，颜色格式仍为 RGB888 24-bit。

| 项 | 1.0.0 | 1.0.1 |
| --- | --- | --- |
| PCLK | 18 MHz | 25 MHz |
| HSYNC（脉宽 / 后肩 / 前肩） | 1 / 40 / 20 | 4 / 8 / 8 |
| VSYNC（脉宽 / 后肩 / 前肩） | 1 / 10 / 5 | 4 / 8 / 8 |
| 刷新率 | 约 42 Hz | 约 61 Hz |

Refresh is about 61 Hz: 25 MHz / ((800 + 4 + 8 + 8) × (480 + 4 + 8 + 8)).

## 1.0.0

- Initial release of the UES31S043H800V480C-U BSP.
