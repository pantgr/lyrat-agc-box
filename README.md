# ESP32-LyraT V4.3 AGC Box

Turn an **ESP32-LyraT V4.3** board into a TV audio AGC (Automatic Gain Control) box using ESPHome and the onboard ES8388 codec.

> ⚠️ **Board version matters.** This project targets the **V4.3** revision of the LyraT board specifically. Other revisions (V4, V4.2, LyraT-Mini) have different GPIO assignments, different button layouts (the LyraT-Mini for instance has no REC/MODE tact buttons), and may use different codecs or DACs. Do not flash this firmware on a non-V4.3 board without first cross-referencing the schematic.

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
- **Privacy Mode** — WiFi disabled by default at boot; **REC** onboard button (GPIO36) toggles WiFi on/off on demand
- **Buffer health diagnostics** — 3 sensors (`Loopback Reads OK / Fail / Max Proc us`) expose real-time I2S loopback metrics in HA so you can verify the audio task is actually working at expected rates; useful for forks tweaking audio parameters
- **Web server** on port 80 for quick access (only when WiFi is enabled)

## Privacy Mode (WiFi off by default)

The firmware boots with **WiFi disabled** after the ES8388 init completes. The radio is fully silent until the user presses the onboard **REC** tact button (SW3, GPIO36 / SENSOR_VP). Press REC again to disable WiFi. State does NOT persist across reboot — every power cycle starts in WiFi-off mode by design.

**Why this matters:**

1. **Radio-induced audio noise** — WiFi RF (especially during heavy bursts like OTA, image transfer, mDNS storms) can couple into the LyraT's analog output stage and produce audible buzz/hiss in the AGC chain. With WiFi off during normal listening, this RF source is eliminated entirely. Subjective improvement: cleaner background, lower noise floor on quiet passages.
2. **Boot-loop / WiFi-stack recovery** — if the board ever falls into a boot loop caused by a hardware issue (e.g. cold solder joints on the ESP32 module — see Known Issues) or a corrupted WiFi NVS partition, the WiFi-off-by-default firmware breaks the failure cycle: the device finishes its codec init and goes silent on radio, giving you a stable platform to physically diagnose without WiFi instability piling on top.
3. **Listening privacy** — for users who don't want a constantly-broadcasting device next to their TV when they aren't actively configuring it. Press the button only when you need OTA updates or HA control, otherwise it's a pure analog AGC box.

**Behavior:**
- Boot → ES8388 init runs (~3-5 sec) → WiFi disables automatically
- Device shows **Unavailable** in Home Assistant (expected — not a fault)
- AGC works fully throughout (hardware-resident on the ES8388, host-independent)
- **Press REC short** → WiFi enables in ~10 sec, device returns to HA
- **Press REC short again** → WiFi disables, device goes back to silent
- **Important:** OTA flash is **only possible while WiFi is enabled** — press REC first.

### Stability fixes — required for reliable Privacy Mode

A naive `wifi.disable:` on its own is **not enough** for stable long-term operation. We discovered that the device would silently reboot after random multi-hour intervals while in WiFi-off state (never with WiFi on). The four changes below were all needed before the device became rock-solid stable in Privacy Mode. **If you fork this and only copy the `wifi.disable:` action, expect random reboots.** Copy the full `lyrat.yaml` + `components/i2s_loopback/` for the working baseline.

1. **`reboot_timeout: 0s` on both `wifi:` and `api:`** — without this, ESPHome's built-in "no WiFi connection / no client connected" timers keep counting and eventually trigger an auto-reboot, even when WiFi was deliberately disabled by the user.
2. **Removal of `web_server`, `captive_portal`, and the `wifi.ap` fallback block** — these components run silent retry/listen loops that misbehave when their underlying network interface is intentionally down. They are unnecessary for the AGC use case anyway.
3. **`vTaskDelay(1)` yield in the I2S loopback task's no-data path** — without an explicit yield, the loopback task at priority 5 on core 1 was starving IDLE_1, which stops feeding the task watchdog. The yield only fires when no audio buffer is ready, so it adds zero latency on hot path.
4. **`Reset Reason` (text_sensor) + `Uptime` (sensor) diagnostic entities** — kept permanently in the yaml. They cost almost nothing to run and are invaluable when something does go wrong: the ESP32 RTC register stores the cause of the most recent reset and persists across the WiFi-off period, so when you eventually press REC and reconnect to HA, you immediately see why the last reboot happened.

After all four were applied, the device ran continuously in WiFi-off Privacy Mode without unexpected reboots.

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

1. Copy `components/` folder to your ESPHome config directory (e.g. `/config/esphome/components/`)
2. Copy `lyrat.yaml` to your ESPHome config directory
3. Copy `secrets.yaml.example` to `secrets.yaml` (same folder), then edit and fill in real values for each entry — see the comments inside the file for generation hints (`openssl rand -hex 16` for the OTA password, base64-encoded 32 bytes for the API encryption key, etc.). **Never commit `secrets.yaml` to git.**
4. Flash via ESPHome dashboard (Install → Wirelessly, or via USB cable for the first flash)
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

- **Boot loop on first boot — `wifi:631 Starting` → `POWERON_RESET` — RESOLVED 2026-05-05 (hardware mod):** With ESPHome 2026.4 the implicit default CPU frequency changed from 160 MHz to 240 MHz. On stock LyraT V4.3 boards, the onboard 1117-series 3.3V LDO (LD1117S33 / AMS1117 footprint) cannot supply the microsecond-scale current spike that `esp_wifi_start()` draws when powering up the WROVER-IE PHY/RF subsystem. The rail sags, the chip resets — reporting `POWERON_RESET 0x1` rather than `BROWNOUT_RST`, with **no panic, no stack trace, no error message**. Crash signature in serial:
  ```
  [C][wifi:631]: Starting
  ets Jul 29 2019 12:21:46
  rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
  ```
  ESPHome's `safe_mode` component has a hardcoded `delay(300)` before `App.setup()` ("to allow power to stabilize before Wi-Fi/Ethernet is initialised") which is why the chip sometimes _eventually_ boots after ~10 attempts via safe-mode recovery — but normal boot stays broken without a fix.

  **Strong diagnostic signal:** the on-board 1117 LDO **degrades over time** in this role — on the test bench, the original LDO failed, was replaced with a fresh LD1117/AMS1117 of the same class, and the replacement **also failed** within a short time. Same-part failures across replacements indicate the design is operating the part beyond its comfort zone, not a single defective unit.

  **Verified bisection (2026-05-04, ~10 hours, 3 boards from different suppliers):** ruled out NVS state, PSRAM, `sram1_as_iram`, brownout detector, framework choice (ESPHome / Arduino / pure ESP-IDF all crash identically), IDF version, GPIO0/ES8388-MCLK conflict, antenna disconnected, flash speed/mode, low-power TX settings, ESPHome wifi component code path. We **also** misdiagnosed power early: a bench supply on the 3V3 rail showed steady 3.3V on a multimeter and we wrongly concluded "not power." That bench supply lacked the transient loop response to follow the microsecond PHY init spike — its DC voltage was perfect but it sagged on the spike. **Steady-state DC voltage ≠ proven good power.**

  **HARDWARE FIX (verified 2026-05-05 on 2 different LyraT V4.3 boards):**
  1. De-solder the on-board 1117 3.3V LDO completely
  2. Inject regulated power into the 3V3 rail from any external switching regulator with adequate transient response. **Two confirmed working modules** (sub-€2 on Aliexpress):
     - **XL6009** DC-DC step-up boost converter (4A, LM2577 IC, 5–32V → 5–50V adjustable)
     - **LM2596S** DC-DC step-down buck converter (adjustable output)
  3. Adjust output for ~3.3V at the ESP32 3V3 pin (measured 3.320V — rock solid under all loads)
  4. Result: chip boots cleanly first try at default 240 MHz, no boot loops EVER, all features (WiFi, PSRAM, codec, loopback) work normally

  Both buck and boost topologies work — the switching topology itself isn't the magic, just the fact that a dedicated SMPS module has fast enough transient response to follow the PHY init current spike. The on-board AMS1117 is the bottleneck.

  **Software workaround (default in this repo's `lyrat.yaml` — for users who do not want to do the hardware mod):** Pin `cpu_frequency: 160MHz`. Lower base current → smaller WiFi PHY current spike → fits within the dying 1117's degraded supply budget. AGC use case has _massive_ headroom at 160 MHz; you lose nothing functionally. This is a true band-aid that lets the unmodified board run reliably enough — but the 1117 LDO will continue to age, so plan to do the hardware mod eventually.

## Tips

- Set your TV/source DAC output to **maximum** for best ALC performance
- GENERIC preset works best for most TV content
- Add bypass capacitors to the LyraT power supply for cleaner idle audio
- The I2S loopback task runs on core 1 at priority 5 with 8KB stack

## Buffer sizing diagnostics

Three template sensors expose live metrics from the loopback task. They are zero-cost (12 bytes RAM, ~5 instructions per cycle) and let you verify the audio path is healthy. Useful when tweaking `dma_desc_num` / `dma_frame_num` in the loopback component or when running custom DSP that changes per-cycle CPU time.

| Sensor | What it means | Healthy value | Bad value |
|--------|---------------|---------------|-----------|
| `Loopback Reads OK` | Cumulative count of successful I2S read+write cycles since boot. Increments at the audio buffer fill rate. | ~375/sec for default 1024-byte task buffer @ 48 kHz / 32-bit / stereo | Stuck or growing slower than expected → CPU bottleneck or DMA stall |
| `Loopback Reads Fail` | Cumulative count of `i2s_channel_read` timeouts/errors. | **0 forever** | Any growth → DMA underrun, buffers too small for current CPU/task scheduling |
| `Loopback Max Proc us` | Maximum time (μs) any single read+process+write cycle has taken since boot. Includes block-on-read time. | <2,667 μs steady state for default buffer; transient boot spike around 30 ms is normal | Sustained >5,000 μs → heavy DSP load, consider larger buffers or simpler processing |

**Interpretation rule of thumb:**
1. If `Reads Fail = 0` and `Max Proc us < 2,667`, your buffers are sized correctly. No tuning needed.
2. If `Reads Fail` grows but `Max Proc us` is normal, your task is being preempted (other ESPHome components stealing CPU). Increase task priority or reduce other work.
3. If `Max Proc us` exceeds 2,667 μs steady state, your processing is too slow per cycle. Either make processing faster (vectorize, reduce sample rate, simpler DSP) or increase `dma_frame_num` to give yourself more time per cycle.

The reference setup (TV audio passthrough with optional balance) runs at ~50 μs per cycle steady state — 50× headroom over the 2,667 μs deadline. That's why the boot spike around 30 ms doesn't cause failures: the DMA ring (43 ms total) absorbs it.

**Test script (35 sec capture of all 3 sensors via aioesphomeapi):**
```bash
timeout 35 esp-logs.sh lyrat 2>&1 | grep -E 'Loopback|Reset Reason'
```

## Hardware references

The `hardware/` folder contains official PDFs useful for anyone hacking on this:

- 📄 [`ES8388_user_guide.pdf`](hardware/ES8388_user_guide.pdf) — Everest Semi ES8388 codec full register reference (28 pages). **Read this before modifying any I²C register write in `lyrat.yaml`** — every register/bit explanation is here.
- 📄 [`ES8388_datasheet.pdf`](hardware/ES8388_datasheet.pdf) — ES8388 chip datasheet (electrical specs, pinout, block diagram). Companion to the user guide.
- 📄 [`ESP32-LyraT_v4.3_component_layout.pdf`](hardware/ESP32-LyraT_v4.3_component_layout.pdf) — official Espressif PCB component layout (top + bottom side). Use it to find C62, C64, C65, C66 for the aux input mod, or to locate any other component on the board.

## Roadmap

Planned enhancements that will make this even more standalone-friendly. Contributions welcome:

- **Onboard tactile button control (TOUCH0–TOUCH6)** — the LyraT has 6 unused tact buttons. Mapping ideas: `TOUCH0–3` → cycle ALC presets (OFF / GENERIC / MUSIC / VOICE), `TOUCH4–5` → volume up/down, `TOUCH6` → mute or recall saved profile. This makes the box fully controllable without HA, WiFi, or any host.
- **Pre-built `.bin` release without WiFi/HA** — once button control is in place, a "no-WiFi" firmware binary will be published in the GitHub Releases tab. Just flash, plug between TV and speakers, control with the onboard buttons. Zero setup, zero network exposure.
- **User-editable AGC profiles on SD card** — the LyraT's onboard SD slot is currently unused. Plan: on boot, the firmware scans the SD root for `*.profile` (or `profiles.json`) files and loads each as a selectable preset. Users edit profiles on a PC (any text editor) and just plug the card back in — no re-flashing, no WiFi, no toolchain. Example use cases: `movies.profile` (slow attack, gentle compression), `news.profile` (fast attack, aggressive leveling for voice), `late-night.profile` (heavy boost for quiet listening, hard ceiling). Profile schema would expose all ES8388 ALC registers (target, attack, decay, max gain, min gain, hold, noise gate) plus the input/output trim values. Buttons cycle through whichever profiles the SD has — so a user can ship custom profiles to friends just by sharing a tiny text file.
- **SD card SPL/level logging (optional)** — log peak/RMS levels to a CSV on SD over time, so users can analyze their listening environment and tune profiles based on real data.
- **Per-input gain auto-calibration** — auto-detect channel imbalance on a known reference tone and write the trim values to flash. Eliminates the manual oscilloscope step.

## License

MIT - Free to use, modify, and distribute.

## Credits

Developed with Claude Code (Anthropic) through extensive hardware testing and ES8388 datasheet analysis.
