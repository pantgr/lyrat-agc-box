# ESP32-LyraT AGC Box

Turn an **ESP32-LyraT V4.3** board into a TV audio AGC (Automatic Gain Control) box using ESPHome and the onboard ES8388 codec.

**What it does:** Automatically levels TV audio - boosts quiet dialog, cuts loud scenes. No more grabbing the remote during movies!

> **No Home Assistant? No problem.** The audio leveling itself runs in **hardware on the ES8388 codec**, so the AGC starts working the moment the board powers on — even with no WiFi, no HA, no host of any kind. Home Assistant is only used for live control (preset switching, volume sliders, balance trim). After you flash the firmware once with your preferred preset and balance baked in, the box is fully autonomous: plug it between TV and speakers, give it 5V power, done.

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

1. Copy `components/` folder to your ESPHome config directory
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

## Recommended Hardware Mod — Remove Aux Input Capacitors C62, C64, C65, C66

The LyraT V4.3 ships with four capacitors on the Aux/LINE2 input path that seriously degrade analog quality. This is an acknowledged Espressif board defect — see the official forum thread: https://esp32.com/viewtopic.php?t=12407

| Cap | Value | What it does on the board | Effect of removal |
|-----|-------|---------------------------|-------------------|
| **C62** | 0.1 µF | Bridges L and R channels | Restores stereo channel isolation (kills crosstalk) |
| **C64** | 0.1 µF | Shunts L input to Au_ground | Restores high-frequency response on L |
| **C65** | 0.1 µF | Shunts R input to Au_ground | Restores high-frequency response on R |
| **C66** | — | Degrades R input | Cleans up right channel |

📋 **Physical locations of these capacitors** on the PCB are shown in the official Espressif component layout: [hardware/ESP32-LyraT_v4.3_component_layout.pdf](hardware/ESP32-LyraT_v4.3_component_layout.pdf) (page 1 = top side, page 2 = bottom side). Use this to find C62, C64, C65, C66 before desoldering.

Desoldering all four gives a **noticeably cleaner** input with proper stereo separation and full frequency response. Both of my boards have this mod applied. **Recommended** if you're using the Aux input for anything serious. Do NOT try to compensate for these caps in software — it's a pure analog-stage issue.

## Known Issues

- **Channel imbalance after the cap mod:** Some residual L/R difference may remain from board tolerances and/or your downstream chain (preamp tubes, etc.). Trim with the Input L/R sliders or, if the drift is downstream of the LyraT, on the Output L/R sliders.
- **DAC digital volume defaults to -96dB (muted):** Registers 0x1A/0x1B must be set to 0x00 explicitly.
- **ALC ALCSEL bits:** Register 0x12 bits[7:6] must be 11 for stereo ALC. Values like 0x38 or 0x08 have ALC OFF!
- **Brownout on boot:** Requires `CONFIG_ESP32_BROWNOUT_DET_LVL_SEL_0: y` in sdkconfig.
- **Boot loops on internal-antenna boards (cold solder joints):** LyraT boards shipped with an internal/PCB-antenna ESP32 module variant (i.e. ESP32-WROVER, *not* the -IE/IPEX version) can hit random boot loops, WiFi dropouts, and "fixes itself" behavior caused by cold solder joints between the ESP32 module and the LyraT carrier PCB. It looks like a firmware/OTA/WiFi bug but it isn't — it's mechanical. **Quick diagnostic test:** with the board powered, gently press down on the corner of the ESP32 module near the antenna — if it suddenly boots / WiFi reconnects / gets stable, you've confirmed cold solder joints. **Fix:** re-flow the ESP32 module pads on the carrier board with a hot air station (or a careful iron) and the issue disappears. If you have the IPEX/external-antenna variant you're far less likely to see this.

## Tips

- Set your TV/source DAC output to **maximum** for best ALC performance
- GENERIC preset works best for most TV content
- Add bypass capacitors to the LyraT power supply for cleaner idle audio
- The I2S loopback task runs on core 1 at priority 5 with 8KB stack

## Hardware references

The `hardware/` folder contains official PDFs useful for anyone hacking on this:

- 📄 [`ES8388_user_guide.pdf`](hardware/ES8388_user_guide.pdf) — Everest Semi ES8388 codec full register reference (28 pages). **Read this before modifying any I²C register write in `lyrat.yaml`** — every register/bit explanation is here.
- 📄 [`ES8388_datasheet.pdf`](hardware/ES8388_datasheet.pdf) — ES8388 chip datasheet (electrical specs, pinout, block diagram). Companion to the user guide.
- 📄 [`ESP32-LyraT_v4.3_component_layout.pdf`](hardware/ESP32-LyraT_v4.3_component_layout.pdf) — official Espressif PCB component layout (top + bottom side). Use it to find C62, C64, C65, C66 for the aux input mod, or to locate any other component on the board.

## License

MIT - Free to use, modify, and distribute.

## Credits

Developed with Claude Code (Anthropic) through extensive hardware testing and ES8388 datasheet analysis.
