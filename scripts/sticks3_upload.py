# Upload support for the M5StickS3 (and any other native-USB ESP32-S3).
#
# The chip's USB-C goes to the OTG PHY, not a USB-UART bridge, so esptool's
# DTR/RTS handshake reaches nothing: `--before default_reset` cannot put the
# board into download mode, and `--after hard_reset` cannot boot it back out.
# Left alone, `pio run -t upload` fails to connect, and a manual upload leaves
# the board sitting in the bootloader until someone presses the power button.
#
# Two substitutions fix both ends:
#
#   entering  Arduino's TinyUSB CDC reboots to the bootloader when the host
#             opens the port at 1200 baud and drops DTR — the Leonardo touch.
#             The device then re-enumerates under a different port name, so
#             UPLOAD_PORT is re-resolved afterwards.
#   leaving   `--after watchdog_reset` restarts the chip from the inside
#             instead of over a control line it cannot hear.
#
# Wired up from platformio.ini as `extra_scripts = pre:scripts/sticks3_upload.py`.

Import("env")

import subprocess
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - PlatformIO ships pyserial
    serial = None
    list_ports = None

ESPRESSIF_VID = 0x303A
SETTLE_S = 0.3
REENUMERATE_TIMEOUT_S = 10.0
POLL_S = 0.25


def _esp_ports():
    if list_ports is None:
        return []
    return sorted(p.device for p in list_ports.comports() if p.vid == ESPRESSIF_VID)


def _touch_1200(port):
    """Open at 1200 baud with DTR low, which asks the sketch to reboot into
    the bootloader. The close almost always raises as the device drops off
    the bus mid-call — that is the success path, not an error."""
    try:
        s = serial.Serial(port, 1200)
        s.dtr = False
        s.rts = False
        time.sleep(SETTLE_S)
        s.close()
    except Exception:
        pass


def _wait_for_new_port(before, deadline):
    while time.time() < deadline:
        now = _esp_ports()
        fresh = [p for p in now if p not in before]
        if fresh:
            return fresh[0]
        # Same name reused: the node vanished and came back, so it is new too.
        if now and now != before:
            return now[0]
        time.sleep(POLL_S)
    return None


def before_upload(source, target, env):
    _swap_reset_flags(env)

    if serial is None:
        print("[sticks3] pyserial unavailable; skipping download-mode touch")
        return

    before = _esp_ports()
    if not before:
        print("[sticks3] no Espressif USB device found — is the stick plugged in?")
        return

    port = env.subst("$UPLOAD_PORT") or before[0]
    print("[sticks3] touching %s at 1200 baud to enter download mode" % port)
    _touch_1200(port)

    new_port = _wait_for_new_port(before, time.time() + REENUMERATE_TIMEOUT_S)
    if new_port:
        print("[sticks3] download mode on %s" % new_port)
        env.Replace(UPLOAD_PORT=new_port)
        return

    # No re-enumeration: either the board was already sitting in the
    # bootloader, or it is running firmware that does not honour the touch.
    # Either way the existing port is the best guess — esptool reports a
    # clearer failure than anything we could raise here.
    still = _esp_ports()
    if still:
        print("[sticks3] port did not change; continuing with %s" % still[0])
        env.Replace(UPLOAD_PORT=still[0])


def _swap_reset_flags(env):
    """Must run from the pre-action, not at script load: the platform's own
    builder sets UPLOADERFLAGS after `pre:` scripts are evaluated, so anything
    replaced at load time is overwritten before the upload runs."""
    flags = list(env.get("UPLOADERFLAGS", []))
    changed = False
    for i, flag in enumerate(flags[:-1]):
        if flag == "--before" and flags[i + 1] != "no_reset":
            flags[i + 1] = "no_reset"
            changed = True
        elif flag == "--after" and flags[i + 1] != "no_reset":
            flags[i + 1] = "no_reset"
            changed = True
    if changed:
        env.Replace(UPLOADERFLAGS=flags)
        print("[sticks3] reset flags: --before no_reset --after no_reset")


def after_upload(source, target, env):
    """Boot the freshly flashed image.

    The reset is a separate, failure-tolerant invocation rather than
    `--after watchdog_reset` on the write itself. A watchdog reset drops the
    USB device the moment it lands, so esptool raises on the way out even
    though the flash verified — and that non-zero exit would fail the build,
    hiding real flash errors behind a cosmetic one. Splitting them keeps the
    write's exit code meaningful and lets the reset be best-effort."""
    port = env.subst("$UPLOAD_PORT")
    if not port:
        return
    cmd = [
        env.subst("$PYTHONEXE"), env.subst("$UPLOADER"),
        "--chip", "esp32s3", "--port", port, "--no-stub",
        "--before", "no_reset", "--after", "watchdog_reset", "read_mac",
    ]
    try:
        subprocess.run(cmd, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=30)
    except Exception:
        pass
    print("[sticks3] watchdog reset issued — the board boots on its own")


# uploadfs needs the identical treatment: tools/flash_character.py drives it
# to push a character pack over USB, and it reaches the same chip through the
# same portless USB stack.
for _target in ("upload", "uploadfs"):
    env.AddPreAction(_target, before_upload)
    env.AddPostAction(_target, after_upload)
