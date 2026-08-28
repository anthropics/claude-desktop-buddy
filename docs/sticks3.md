# M5StickS3 notes

Board-specific behaviour, and what the desktop bridge actually puts on the
wire. Everything here was measured on hardware — an ESP32-S3-PICO-1 rev v0.2
with 8MB flash and 8MB octal PSRAM — against Claude for macOS 1.37937.1.

## Flashing

```bash
pio run -e m5stack-sticks3 -t upload
```

No button presses. `scripts/sticks3_upload.py` handles the two things that
otherwise make this board awkward, and the rest of this section explains why
they are needed.

The StickC Plus reaches its ESP32 through a USB-UART bridge, so esptool can
drive the chip's EN and BOOT pins over DTR and RTS. The StickS3's USB-C goes
straight to the SoC's OTG PHY. There is no bridge and there are no control
lines, so both halves of esptool's normal choreography fail silently:

| | StickC Plus | StickS3 |
| --- | --- | --- |
| enter download mode | `--before default_reset` | 1200-baud touch with DTR low |
| leave download mode | `--after hard_reset` | `--after watchdog_reset` |

The touch is Arduino's TinyUSB CDC convention, the same one a Leonardo uses:
opening the port at 1200 baud and dropping DTR asks the running sketch to
reboot into the bootloader. The device then re-enumerates under a different
port name, so the upload port has to be re-resolved afterwards.

The watchdog reset restarts the chip from the inside rather than over a
control line it cannot hear. It is issued as a separate, failure-tolerant
step: the reset drops the USB device the instant it lands, so esptool raises
on the way out even though the flash verified. Folding it into the write
would turn that cosmetic error into a failed build and hide real flash errors
behind it.

If a board is running something that does not honour the touch — factory
UIFlow2, or an image whose USB never came up — enter download mode by hand:
**hold the side button about two seconds and release**; the green LED blinks
when it takes.

### Recovering the factory firmware

Nothing here is one-way. Dump the whole flash before the first upload and it
can go back at any time:

```bash
esptool.py --chip esp32s3 --port <port> read_flash 0 0x800000 factory.bin
esptool.py --chip esp32s3 --port <port> write_flash 0x0 factory.bin
```

### One thing to watch

Close any serial monitor before uploading. A process holding the port makes
esptool fail partway through a large write with `Packet content transfer
stopped` or `device reports readiness to read but returned no data` — neither
of which points at the actual cause, and both of which look like flaky
hardware.

## What differs from the StickC Plus

Both boards build from one source tree; M5Unified detects which one it is at
runtime. **M5Unified 0.2.12 or newer is required** — older versions have no
`board_M5StickS3` and will compile fine but never light the display.

| | StickC Plus | StickS3 |
| --- | --- | --- |
| MCU | ESP32-PICO | ESP32-S3-PICO-1 (dual-core, 8MB PSRAM) |
| PMIC | AXP192 | M5PM1 |
| IMU | MPU6886 | BMI270 |
| Display | ST7789 135x240 | ST7789P3 135x240 |
| Audio | piezo buzzer | ES8311 codec + AW8737 amp + speaker + mic |
| RTC | on-board | none — the clock runs off the system clock |

The display is the same size on both, so nothing in the drawing code needed
to change.

### The LED is not on a GPIO

The StickS3's indicator sits below the power button, visible through a seam
in the case. It hangs off the M5PM1's `LED_EN` rail — `PWR_CFG` bit 4, over
the internal I2C bus — not off a pin. GPIO10, which drives the LED on the
StickC Plus, is Grove Port A here.

M5Unified's StickS3 power init claims PM1 GPIO0 for the power button and
leaves `LED_EN` alone, so the firmware owns it outright:
`M5.Power.M5pm1.setLedEnLevel(on)`.

### USB mode must be 0

`ARDUINO_USB_MODE=1` selects the USB-Serial/JTAG peripheral. This board's PHY
is wired for OTG — esptool reports `USB mode: USB-OTG` — so with mode 1
nothing enumerates and `Serial` writes vanish. Mode 0 puts TinyUSB on the OTG
peripheral, which enumerates as `Espressif ESP32_S3_DevKitC_1_N8` and gives
back both the serial console and the 1200-baud touch.

### The speaker needs the PMIC

`M5.Speaker` works, but only because M5Unified's StickS3 enable callback
switches on the AW8737 through PM1 GPIO3 and initialises the ES8311 over I2C.
It is not a pin-toggle buzzer; volume is a real range, and 180/255 — the
value this firmware used to hardcode — is loud in a quiet room.

## Pairing

The device advertises as `Claude-XXXX`, the last two bytes of its Bluetooth
MAC, which is the base MAC with 1 added to the final octet.

Pairing uses LE Secure Connections with passkey entry: the device displays a
six-digit code and macOS prompts for it. This is what buys MITM protection,
and it matters, because transcript snippets and tool-call hints cross this
link in the clear otherwise.

**Re-pairing needs both sides cleared.** The Hardware Buddy window's
**Forget** button drops the desktop's record and tells the device to erase
its bonds, but macOS keeps its own. With the device's bond gone and the
host's still present, macOS keeps offering a key the device no longer knows,
and every attempt fails with `auth FAIL` on the device and `pair:
result=false` on the desktop. No passkey is ever generated, which makes it
look like a pairing-mode problem rather than a stale bond.

Clear the host side too, in **System Settings → Bluetooth → ⓘ → Forget This
Device**. `system_profiler SPBluetoothDataType` will list the device under
"Not Connected" while the stale bond is still there.

## What the desktop actually sends

REFERENCE.md documents the protocol. These are the parts that only show up
once you watch real traffic.

**`total` is not a live count.** It is every non-archived session the desktop
knows about, across Claude Code and Cowork both. A working machine reports
several dozen. `running` and `waiting` are the fields worth reacting to.

**There is no `completed` field.** The desktop's heartbeat carries exactly
`total`, `running`, `waiting`, `msg`, `entries`, `tokens`, `tokens_today` and
an optional `prompt`. Nothing announces that a task finished. To notice that,
watch `running` fall back to zero — and debounce it, because it dips between
turns inside a single agent session.

**A session waiting on a question looks identical to one that finished.**
`AskUserQuestion` arrives as an ordinary tool-permission request, so it does
raise `waiting` and the approval panel. But approving it only lets the tool
run; the question itself is then answered on the desktop, and while it waits
there the session is neither running nor waiting. Nothing distinguishes that
from completed work.

**`entries` carries prose, not just commands.** The transcript is the tail of
the session's own messages — assistant text included, in whatever language
the session is conducted in. It is not a log of tool invocations, and it is
routinely non-ASCII.

**Size the whole receive path for CJK, not just the entry slots.** The
desktop keeps the last 8 messages of the most recently active session and
slices each to 88 characters. That is 88 bytes of ASCII but 264 of CJK, so
one snapshot reaches about 2.2KB on the wire — measured at 2212 bytes for the
worst case the protocol permits. Every buffer it passes through has to hold
it whole:

| | was | needs |
| --- | --- | --- |
| BLE RX ring | 2048 | 8192 |
| JSON line buffer | 1024 | 4096 |
| USB CDC RX queue | 256 (Arduino default) | 4096 |

Undersizing any of them fails the same way, and the way is nasty: bytes are
dropped mid-line, the JSON no longer parses, and a parse failure discards the
*entire* snapshot — session counts, `msg`, pending prompt and all. Nothing is
logged, nothing errors, and after 30s of that the device decides the bridge is
gone and displays "No Claude connected" while the desktop insists it is
connected. Sizing for a latin transcript and testing in English hides this
completely.

Drop a byte on the floor loudly: count what the ring discards and report an
overlong line rather than handing its truncated head to the parser.

**Chat conversations are not included.** Only Claude Code and Cowork sessions
have the `pendingToolPermissions` the bridge reads.

## Power

The 250mAh cell is small for what this board runs — a dual-core S3 with
PSRAM, BLE advertising continuously, and a backlit LCD — so the margin
between draw and charge is thin enough that configuration mistakes flip the
sign. All three below were measured on hardware.

**Turn the Grove rail off.** `M5.begin()` defaults `cfg.output_power` to
true, which enables the PM1's 5V boost converter for the Grove port. On a
firmware that never touches that port it runs unloaded off the battery, and
it costs enough to cancel out the charge current: measured on USB, a stick
with the boost on drifted down from 3.6V to 3.36V across an afternoon while
`isCharging()` read true the whole time, with battery voltage swinging ±60mV
sample to sample. With `cfg.output_power = false` the same stick climbed
steadily and the swing dropped to ±6mV.

That failure mode is quiet, which is what makes it worth knowing: VBUS reads
5.1V, the charge-status pin says charging, `PWR_CFG` has `CHG_EN` set, and
the battery still goes down.

**Clock down.** The StickC Plus target sets `board_build.f_cpu =
160000000L`. Without it the S3 runs at 240MHz — more than the older, slower
board, for a workload that is idle most of the time.

**Do not drive the LED from the render loop.** `LED_EN` is bit 4 of the same
`PWR_CFG` register as `CHG_EN`, `DCDC_EN`, `LDO_EN` and `BOOST_EN`, so every
LED write is an I2C read-modify-write of the PMIC's power configuration. On
the StickC Plus the same call is a `digitalWrite` and costs nothing, which
makes this easy to miss when porting. Deduplicate at the call site rather
than inside the hardware shim — `compat.h` is included by two dozen
translation units, so a `static` latch there would give each of them a
private copy of state that mirrors one hardware register.

`bat.mA` is always 0 on this board — the PM1 does not report battery current
through M5Unified — so charge state has to be read from `isCharging()` (PM1
GPIO0, low means charging) and confirmed by watching the voltage trend.
`getChargeCurrent()` returns 0 as well.

## When it wedges

`setup()` subscribes `loop()` to the task watchdog. The Arduino core leaves
the loop task unwatched by default, which means a wedged main loop takes the
UI, the BLE bridge and the serial console with it while the USB peripheral
keeps enumerating from its own task — the stick looks alive to the host and
needs someone to physically press the power button. That happened once here,
with nothing to explain it.

The timeout is 5s and `CONFIG_ESP_TASK_WDT_PANIC` is set, so a wedge now
panics, writes a core dump to the partition reserved for one, and reboots.
Measured end to end: 5.5s from wedge to reset, back on the bus 2s later.

The factory-reset path unsubscribes first — `LittleFS.format()` alone runs
well past 5s on a 3.94MB partition — and ends in `ESP.restart()` anyway.

To read a dump back, with the board in download mode:

```bash
esptool.py --chip esp32s3 --port <port> read_flash 0x7F0000 0x10000 core.bin
pip install esp-coredump
esp-coredump --chip esp32s3 info_corefile \
  --gdb <toolchain>/bin/xtensa-esp32s3-elf-gdb \
  --core core.bin --core-format raw \
  .pio/build/m5stack-sticks3/firmware.elf
```

The `loopTask` thread's frame #0 is where it stopped. Verified against a
deliberate `for(;;)` — the backtrace named the exact source line.

## Serial output

With USB mode 0 the console works, and the firmware logs state changes rather
than a stream:

```
[hb] total=49 running=1 waiting=1 msg='approve: AskUserQuestio'
[prompt] aa029abe-c7fd-421a-9761-44d0cb3d00d7 tool='AskUserQuestion'
{"cmd":"permission","id":"aa029abe-...","decision":"once"}
[prompt] (cleared) tool=''
[alert] all sessions quiet
```

`[hb]` fires only when the session counts move, so it is a usable trace of
what the desktop believes is happening. The round trip from pressing **A** to
the panel clearing measures about 300ms.
