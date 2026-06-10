# StickS3 Voice Mic — setup guide

Turn an **M5Stack StickS3** into a push-to-talk dictation microphone for your
Mac. You hold a button on the stick, speak, and the text lands in whatever app
has focus — transcribed by **Doubao IME** (豆包输入法) or any STT tool that can
listen to a virtual audio input.

This build replaces the Claude Code buddy UI on the StickS3 with a dedicated
voice-mic UI. It is a general-purpose voice input device, not tied to any one
IDE or agent.

```
StickS3 ES8311 mic ──IMA ADPCM──▶ BLE NUS (debug, unencrypted)
        │                              │
   [A button PTT]                      ▼
                          tools/sticks3_voice.py  (this bridge)
                              │                │
                  decode → BlackHole 2ch    synthesize PTT hotkey
                  (virtual mic device)      (right-Cmd via Quartz)
                              │                │
                              ▼                ▼
                     Doubao IME (input device = BlackHole, 长按触发)
                              │
                              ▼
                     text typed into the focused app
```

## What you need

- M5Stack StickS3 (SKU K150) flashed with the `m5stack-sticks3-claude` env
  (see [Firmware](#firmware) below)
- macOS (the bridge uses CoreBluetooth + Quartz + CoreAudio)
- [BlackHole 2ch](https://github.com/ExistentialAudio/BlackHole) — free
  virtual loopback audio device
- Doubao IME (豆包输入法) or any dictation tool that lets you pick its input
  device and trigger it with a hotkey

## Firmware

The voice-mic firmware is the standard StickS3 env with mic capture on
(`-DBUDDY_S3_MIC_CAPTURE=1`, already enabled in `platformio.ini`):

```bash
pio run -e m5stack-sticks3-claude -t upload --upload-port /dev/cu.usbmodem2101
```

(The port name varies; `ls /dev/cu.usbmodem*` after plugging in. The StickS3
enumerates on its native USB-Serial-JTAG.)

After boot the stick shows the **VOICE MIC** screen and advertises over BLE
as `Claude-S3-XXXX` (last 2 MAC bytes). The stick is intentionally silent —
the ES8311 speaker path is disabled so the codec is mic-only (this also avoids
a TG1 interrupt-watchdog reboot; see the comment block in
`src/audio_capture.cpp:audioCaptureInit`).

## Install the bridge (host side)

### 1. BlackHole virtual audio device

```bash
brew install blackhole-2ch
```

> The installer needs an interactive `sudo` — run it in a real terminal, not
> through an agent's non-interactive shell. Verify it shows up afterwards:
> `system_profiler SPAudioDataType | grep -i blackhole`

### 2. Python environment

Any Python ≥3.9 venv works. Dependencies:

```bash
python3 -m venv ~/.sticks3-voice-venv
~/.sticks3-voice-venv/bin/pip install bleak sounddevice numpy pyobjc-framework-Quartz
```

(If you already run the cc-bridge daemon, its venv at `~/.cc-bridge/venv`
plus `pip install sounddevice numpy` also works.)

### 3. macOS permissions (both are mandatory)

| Permission | Who needs it | Symptom when missing |
|---|---|---|
| **Bluetooth** (Privacy & Security → Bluetooth) | the **terminal app** that launches the bridge (Terminal/iTerm/Warp/…) | every CoreBluetooth call dies with SIGABRT (exit 134) — looks exactly like "scan finds no devices". `blueutil -p` aborting with the same error confirms it. |
| **Accessibility** (Privacy & Security → Accessibility) | the venv's `python3` binary | BLE works, audio flows, but the synthetic right-Cmd never triggers Doubao. The first keystroke attempt pops the grant dialog. |

### 4. Doubao IME

In Doubao 设置 (语音输入):

1. **输入设备** (input device) → `BlackHole 2ch`
2. **触发方式** → 长按右 Command (hold right-Cmd) — this matches the bridge's
   default `S3_VOICE_PTT=hold` + keycode 54

### 5. Run it

```bash
~/.sticks3-voice-venv/bin/python -u tools/sticks3_voice.py
```

Expected output within ~15 s:

```
scanning for Claude-S3-* ... (PTT style=hold key=54)
connecting Claude-S3-FA49 <uuid>
connected. Do the stick PTT gesture (tap A, then press+hold A) and speak.
```

The bridge auto-reconnects if the stick reboots or goes out of range. USB is
only needed for flashing/charging — the voice link is pure BLE, the stick
works on battery.

## Using the stick

| Gesture | Action |
|---|---|
| **tap A, then press & hold A** | talk — REC + timer + live VU bar while held; release to stop |
| **B short press** | send **Enter** (submit the dictated text) |
| **B long hold** | undo — **Cmd+A + Delete** clears the field (IME-committed text is not on the app's undo stack, so plain Cmd+Z would not work) |

Screen elements: green dot + `Mac` = BLE connected; battery gauge top-right of
the hint bar; the VU bar turns **amber → red** as the level approaches
clipping — if it pins red constantly the mic gain is too hot for good STT.

## Configuration (env vars)

| Var | Default | Meaning |
|---|---|---|
| `S3_VOICE_DEVICE` | `BlackHole` | output device name substring to play decoded audio into |
| `S3_VOICE_KEYCODE` | `54` (right Cmd) | PTT hotkey to synthesize; `61` = right Option; `0` disables |
| `S3_VOICE_PTT` | `hold` | hotkey style: `hold` (keydown↔keyup follows the gesture), `tap`, `double_tap` |
| `S3_VOICE_MONITOR` | `0` | `1` = also play to the default speaker so you can hear yourself |

## Troubleshooting

- **"scanning … device not found" / silent exit** — 9 times out of 10 this is
  the Bluetooth TCC permission (table above), not the stick. Check that first.
- **Do not run other BLE scanners while the bridge is connected** — concurrent
  `BleakScanner`s destabilize macOS `bluetoothd` and cause link flapping. If
  the link starts flapping, toggle Bluetooth off/on in Control Center.
- **`Peer removed pairing information` (CBErrorDomain 14)** — re-flashing
  wiped the stick-side bond but macOS kept its half. System Settings →
  Bluetooth → forget the stale entry (it shows under its GAP name).
- **Stick connected but A does nothing** — the gesture is *tap then hold*,
  not a plain hold; and it only arms from the idle screen.
- **Audio reaches BlackHole but Doubao never starts listening** — Accessibility
  permission missing, or Doubao's hotkey isn't 右 Command. The bridge's
  keystrokes are HID-system-state Quartz events specifically so HID-level
  hotkey listeners (like Doubao's) accept them.
- **Deep diagnostics** — `tools/sticks3_mic_probe.py` connects the same BLE
  service and writes each utterance to a WAV with frame/seq-gap stats; use it
  to verify the mic + radio path independent of audio routing and STT.

## Protocol notes (for porters)

The stick emits newline-delimited JSON on the debug NUS TX characteristic
`b0c2dbe6-cc03-4000-8000-00805f9b34fb` (unencrypted):

- `{"evt":"audio_begin","sr":16000,"frame_raw_bytes":240,"codec":"adpcm_ima"}`
- `{"evt":"audio","seq":N,"crc":…,"data":"<base64 IMA-ADPCM>"}` — 240 raw
  bytes → 480 samples per frame
- `{"evt":"audio_end","seq_total":N,"reason":"ptt_up",…}`
- `{"cmd":"submit"}` / `{"cmd":"undo"}` from the B button

The IMA ADPCM codec matches `src/adpcm.cpp` byte-for-byte; a reference Python
decoder lives in both `tools/sticks3_voice.py` and `tools/sticks3_mic_probe.py`.
