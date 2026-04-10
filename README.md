# ESP32-LyraT AGC Box

Turn an **ESP32-LyraT V4.3** board into a TV audio AGC (Automatic Gain Control) box using ESPHome and the onboard ES8388 codec.

**What it does:** Automatically levels TV audio - boosts quiet dialog, cuts loud scenes. No more grabbing the remote during movies!

## Signal Path

```
TV Audio Out (AUX) -> LINE2 Input -> ES8388 ALC -> I2S Loopback -> DAC -> LOUT1 (Headphone Jack) -> External DAC/Preamp/Speakers
```

## Features

- **24-bit / 48kHz** full-duplex I2S loopback via custom ESPHome component
- **ES8388 ALC** (Automatic Level Control) with 3 presets + OFF, selectable live from Home Assistant
- **4 volume sliders** in HA: Input L/R (ADC digital), Output L/R (analog LOUT1/ROUT1)
- **Per-channel balance correction** for LyraT hardware imbalance (~5dB R>L)
- **All settings persist** across reboot (ESPHome globals)
- **Clean code** - new ESP-IDF 5.x I2S API, zero deprecated warnings
- **WiFi controlled** via Home Assistant + ESPHome
- **Web server** on port 80 for quick access

## ALC Presets

| Preset | Best For | Description |
|--------|----------|-------------|
| **GENERIC** | TV/Movies (default) | Moderate attack/decay, -12dB target, cuts peaks, stable volume |
| **MUSIC** | Music playback | Slow decay (no pumping), -12dB target, max boost +35.5dB |
| **VOICE** | Voice/Podcasts | Fast attack/decay, -4.5dB target, aggressive leveling |
| **OFF** | Bypass | No ALC processing |

## Hardware

- **Board:** ESP32-LyraT V4.3 (ESP32-WROVER-E)
- **Codec:** ES8388 (I2C address 0x10)
- **I2S:** MCLK=GPIO0 (APLL), BCLK=GPIO5, LRCLK=GPIO25, DOUT=GPIO26, DIN=GPIO35
- **I2C:** SDA=GPIO18, SCL=GPIO23

## Installation

1. Copy `custom_components/` folder to your ESPHome config directory
2. Copy `lyrat.yaml` to your ESPHome config directory
3. Create/update your `secrets.yaml`:
   ```yaml
   wifi_ssid: "YourWiFi"
   wifi_password: "YourPassword"
   api_key: "generate-with-esphome"
   ota_password: "your-ota-password"
   ap_password: "fallback-ap-password"
   ```
4. Flash via ESPHome dashboard
5. Adjust Input L/R sliders for channel balance (LyraT has ~5dB hardware imbalance, R louder)
6. Set Output L/R to taste
7. Select ALC preset (GENERIC recommended for TV)
8. Set your TV/source audio output to **maximum** - let the ES8388 handle level control

## ES8388 Register Reference

All codec configuration is done via raw I2C register writes - no external ES8388 component needed. See comments in `lyrat.yaml` for full register documentation.

Key registers:
- **0x10/0x11:** ADC digital volume (Input L/R) - 0=0dB, each step=-0.5dB
- **0x2E/0x2F:** LOUT1/ROUT1 volume (Output L/R) - 0=-45dB, 30=0dB, 33=+4.5dB
- **0x12-0x16:** ALC control registers
- **0x27/0x2A:** Mixer gain (-15dB to +6dB)

## Known Issues

- **LyraT V4.3 channel imbalance:** ~5dB difference (R louder) due to capacitors C62/C64/C65/C66. Corrected via different Input L/R values.
- **DAC digital volume defaults to -96dB (muted):** Registers 0x1A/0x1B must be set to 0x00 explicitly.
- **ALC ALCSEL bits:** Register 0x12 bits[7:6] must be 11 for stereo ALC. Values like 0x38 or 0x08 have ALC OFF!
- **Brownout on boot:** Requires `CONFIG_ESP32_BROWNOUT_DET_LVL_SEL_0: y` in sdkconfig.

## Tips

- Set your TV/source DAC output to **maximum** for best ALC performance
- GENERIC preset works best for most TV content
- Add bypass capacitors to the LyraT power supply for cleaner idle audio
- The I2S loopback task runs on core 1 at priority 5 with 8KB stack

## License

MIT - Free to use, modify, and distribute.

## Credits

Developed with Claude Code (Anthropic) through extensive hardware testing and ES8388 datasheet analysis.
