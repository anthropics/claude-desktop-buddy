#!/usr/bin/env python3
"""StickS3 → BlackHole voice bridge.

Speak into the StickS3; this routes the on-device ES8311 mic into a macOS
virtual audio input so Doubao IME (or any STT bound to that input) transcribes
it into the focused app — the vibecoding dictation loop.

Full setup guide (firmware flash, install, macOS permissions, troubleshooting):
docs/sticks3-voice-mic.md

Chain:  StickS3 ES8311 mic → IMA ADPCM → BLE NUS (debug, unencrypted)
        → this script: decode → play into BlackHole (virtual mic)
        → Doubao IME (input device = BlackHole, triggered by the keystroke below)

A single BLE central can own the stick, so this bridge does BOTH jobs: stream
the audio AND synthesize the PTT trigger key (right-Cmd by default → Doubao
长按). It also relays the stick's B button: short press → Enter (submit),
long hold → Cmd+A + Delete (clear-field undo). Run it INSTEAD of cc-bridge —
the voice-mic stick is not a Claude Code buddy peer.

Quick start:
  1. Install the virtual device:   brew install blackhole-2ch
  2. Doubao 设置 → 输入设备 = BlackHole 2ch; trigger hotkey = 右 Command (长按).
  3. python3 tools/sticks3_voice.py      (then do the stick PTT gesture + speak)

Env:
  S3_VOICE_DEVICE   output device name substring (default "BlackHole")
  S3_VOICE_KEYCODE  PTT keycode (default 54 = right Cmd; 61 = right Option)
  S3_VOICE_PTT      trigger style: hold (default) | tap | double_tap
  S3_VOICE_MONITOR  "1" to ALSO play to the default speaker (hear yourself)
"""
import asyncio, base64, json, os, struct, sys, threading, time
from collections import deque

import numpy as np
import sounddevice as sd
from bleak import BleakScanner, BleakClient

NAME_PREFIX = "Claude-S3-"
DBG_TX = "b0c2dbe6-cc03-4000-8000-00805f9b34fb"   # stick → host NOTIFY (unencrypted)

DEVICE_MATCH = os.environ.get("S3_VOICE_DEVICE", "BlackHole")
PTT_KEYCODE  = int(os.environ.get("S3_VOICE_KEYCODE", "54"))   # 54 = right Cmd
PTT_STYLE    = os.environ.get("S3_VOICE_PTT", "hold")
MONITOR      = os.environ.get("S3_VOICE_MONITOR", "0") == "1"

# ---- PTT key relay (ported from buddy_core/core.py) ----
# Modifier-only keys (kVK 54-63) must be emitted as FlagsChanged, not keyDown.
# CRITICAL: the generic mask (e.g. Command 0x100000) does NOT tell apps WHICH
# side. Apps bound to "右 Command" check the device-dependent bit too, so we OR
# in the left/right device flag (NX_DEVICE?CMDKEYMASK etc.). Without the 0x10
# right-Cmd bit, Doubao (右command) ignores the event.
_DEV = {  # device-dependent side bits
    54: 0x10,    # right Cmd
    55: 0x08,    # left Cmd
    56: 0x02,    # left Shift
    58: 0x20,    # left Option
    59: 0x01,    # left Control
    60: 0x04,    # right Shift
    61: 0x40,    # right Option
    62: 0x2000,  # right Control
}
_GEN = {54: 0x100000, 55: 0x100000, 56: 0x020000, 58: 0x080000, 59: 0x040000,
        60: 0x020000, 61: 0x080000, 62: 0x040000}
_MOD_FLAGS = {kc: _GEN[kc] | _DEV[kc] for kc in _GEN}
try:
    from Quartz import (CGEventCreateKeyboardEvent, CGEventPost, kCGHIDEventTap,
                        CGEventSetType, CGEventSetFlags, kCGEventFlagsChanged,
                        CGEventSourceCreate, kCGEventSourceStateHIDSystemState)
    # CRITICAL: events must be created from a HID-system-state source, not None.
    # Doubao (and many IMEs) detect their hotkey below the session event tap and
    # IGNORE generic synthetic events; a HID-system-state source makes the event
    # look hardware-originated so Doubao honors it. Verified: with source=None the
    # synthetic right-Cmd reaches the session tap but Doubao does nothing; with
    # this source Doubao triggers.
    _SRC = CGEventSourceCreate(kCGEventSourceStateHIDSystemState)
    _HAVE_QUARTZ = True
except Exception as e:
    _HAVE_QUARTZ = False
    _SRC = None
    print(f"[warn] Quartz unavailable ({e}); PTT keystroke disabled", file=sys.stderr)

def _send_key(keycode: int, down: bool):
    if not _HAVE_QUARTZ or keycode <= 0:   # keycode 0 → PTT disabled (audio-only test)
        return
    ev = CGEventCreateKeyboardEvent(_SRC, keycode, down)
    if keycode in _MOD_FLAGS:
        CGEventSetType(ev, kCGEventFlagsChanged)
        CGEventSetFlags(ev, _MOD_FLAGS[keycode] if down else 0)
    CGEventPost(kCGHIDEventTap, ev)

def _send_cmd_key(keycode: int):
    """Press Cmd+<keycode> (e.g. Cmd+Z = undo). HID source so apps honor it."""
    if not _HAVE_QUARTZ:
        return
    for down in (True, False):
        ev = CGEventCreateKeyboardEvent(_SRC, keycode, down)
        CGEventSetFlags(ev, 0x100000)   # kCGEventFlagMaskCommand
        CGEventPost(kCGHIDEventTap, ev)

def ptt_trigger(down: bool):
    """Fire the Doubao trigger per the configured style."""
    if PTT_STYLE == "hold":
        _send_key(PTT_KEYCODE, down)
    elif PTT_STYLE == "tap":
        if down:  # one tap to toggle on; second tap (next down) toggles off
            _send_key(PTT_KEYCODE, True); _send_key(PTT_KEYCODE, False)
    elif PTT_STYLE == "double_tap":
        if down:
            for _ in range(2):
                _send_key(PTT_KEYCODE, True); _send_key(PTT_KEYCODE, False)

# ---- IMA ADPCM decoder (matches src/adpcm.cpp byte-for-byte) ----
STEP_TABLE = [
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,
    88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,
    544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,
    2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
    10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
INDEX_TABLE = [-1,-1,-1,-1,2,4,6,8]

class Adpcm:
    def __init__(self):
        self.pred = 0
        self.idx = 0
    def _nib(self, n):
        step = STEP_TABLE[self.idx]
        delta = step >> 3
        if n & 4: delta += step
        if n & 2: delta += step >> 1
        if n & 1: delta += step >> 2
        if n & 8: delta = -delta
        self.pred = max(-32768, min(32767, self.pred + delta))
        self.idx = max(0, min(88, self.idx + INDEX_TABLE[n & 7]))
        return self.pred
    def decode(self, data: bytes):
        out = np.empty(len(data) * 2, dtype=np.int16)
        i = 0
        for b in data:
            out[i] = self._nib(b & 0x0F); i += 1
            out[i] = self._nib((b >> 4) & 0x0F); i += 1
        return out

# ---- audio sink: decoded PCM → BlackHole via a callback-fed ring ----
class Sink:
    def __init__(self, sr):
        self.sr = sr
        self.buf = deque()            # of int16 ndarrays
        self.lock = threading.Lock()
        self.tee = []; self.pushed = 0
        self.cb_calls = 0; self.cb_out = 0; self.cb_status = 0
        self.dev = self._find_device(DEVICE_MATCH)
        self.mon = self._find_device("") if MONITOR else None
        ch = 2  # BlackHole 2ch — duplicate mono to L/R
        self.ch = ch
        self.stream = sd.OutputStream(
            samplerate=sr, channels=ch, dtype="int16",
            device=self.dev, callback=self._cb, blocksize=0)
        self.stream.start()
        print(f"[sink] streaming {sr} Hz → device #{self.dev} "
              f"({sd.query_devices(self.dev)['name']})")

    @staticmethod
    def _find_device(match):
        for i, d in enumerate(sd.query_devices()):
            if d["max_output_channels"] >= 1 and match.lower() in d["name"].lower():
                return i
        raise RuntimeError(
            f"no output device matching {match!r}. "
            f"Install BlackHole: brew install blackhole-2ch. "
            f"Devices: {[d['name'] for d in sd.query_devices()]}")

    def push(self, pcm: np.ndarray):
        with self.lock:
            self.buf.append(pcm)
            self.tee.append(pcm)         # DEBUG tee of everything pushed
            self.pushed += len(pcm)

    def _cb(self, outdata, frames, t, status):
        if status:
            self.cb_status += 1
        need = frames
        out = np.zeros((frames, self.ch), dtype=np.int16)
        filled = 0
        with self.lock:
            while filled < need and self.buf:
                chunk = self.buf[0]
                take = min(len(chunk), need - filled)
                out[filled:filled+take, 0] = chunk[:take]
                if self.ch > 1:
                    out[filled:filled+take, 1] = chunk[:take]
                filled += take
                if take == len(chunk):
                    self.buf.popleft()
                else:
                    self.buf[0] = chunk[take:]
        outdata[:] = out
        self.cb_calls += 1
        self.cb_out += filled

    def dump_tee(self):
        import wave as _wave
        with self.lock:
            data = np.concatenate(self.tee) if self.tee else np.zeros(1, np.int16)
            self.tee = []
        rms = float(np.sqrt(np.mean(data.astype(np.float64) ** 2)))
        fn = "/Users/txie/.claude/jobs/0988cbcf/tmp/voice_tee.wav"
        with _wave.open(fn, "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(self.sr)
            w.writeframes(data.astype(np.int16).tobytes())
        print(f"[sink] TEE pushed={self.pushed} samples rms={rms:.0f} "
              f"peak={int(np.abs(data).max())} | cb_calls={self.cb_calls} "
              f"cb_out={self.cb_out} cb_status={self.cb_status} -> {fn}")

    def close(self):
        try:
            self.stream.stop(); self.stream.close()
        except Exception:
            pass


def main():
    state = {"sink": None, "dec": None, "active": False, "next_seq": 0,
             "frames": 0, "dropped": 0}
    buf = bytearray()

    def handle_line(line: str):
        line = line.strip()
        if not line or line[0] != "{":
            return
        try:
            o = json.loads(line)
        except Exception:
            return
        cmd = o.get("cmd")
        if cmd == "submit":                   # StickS3 BtnB click → Return (submit)
            _send_key(36, True); _send_key(36, False)   # kVK_Return = 36
            print("[submit] BtnB → Enter")
            return
        if cmd == "undo":                     # StickS3 BtnB hold → clear the input field
            # IME-committed dictation usually isn't on the app's undo stack, so
            # Cmd+Z doesn't remove it (and in a terminal Cmd+Z = suspend). Clear
            # the field instead: select-all then delete — reliable, app-agnostic.
            _send_cmd_key(0)                  # kVK_ANSI_A = 0  → Cmd+A (select all)
            _send_key(51, True); _send_key(51, False)   # kVK_Delete (backspace) = 51
            print("[undo] BtnB hold → Cmd+A + Delete (clear field)")
            return
        evt = o.get("evt")
        if evt == "audio_begin":
            sr = int(o.get("sr", 16000))
            state["dec"] = Adpcm()
            state["next_seq"] = 0; state["frames"] = 0; state["dropped"] = 0
            if state["sink"] is None or state["sink"].sr != sr:
                if state["sink"]: state["sink"].close()
                state["sink"] = Sink(sr)
            state["active"] = True
            ptt_trigger(True)
            print(f"\n=== audio_begin sr={sr} → PTT key {PTT_KEYCODE} DOWN, speak ===")
        elif evt == "audio":
            if not state["active"]:
                return
            seq = int(o.get("seq", 0))
            if seq != state["next_seq"]:
                gap = seq - state["next_seq"]
                if gap > 0:
                    state["dropped"] += gap
            state["next_seq"] = seq + 1
            raw = base64.b64decode(o.get("data", ""))
            state["sink"].push(state["dec"].decode(raw))
            state["frames"] += 1
            if state["frames"] % 40 == 0:
                print(f"  ... {state['frames']} frames streamed "
                      f"(dropped_seq={state['dropped']})")
        elif evt == "audio_end":
            if not state["active"]:
                return
            ptt_trigger(False)
            state["active"] = False
            print(f"=== audio_end reason={o.get('reason')} frames={state['frames']} "
                  f"→ PTT key UP ===")
            if state["sink"]:
                state["sink"].dump_tee()

    def on_notify(_char, data: bytearray):
        buf.extend(data)
        while True:
            nl = buf.find(b"\n")
            if nl < 0:
                break
            line = bytes(buf[:nl]); del buf[:nl+1]
            try:
                handle_line(line.decode("utf-8", "replace"))
            except Exception as e:
                print("parse err:", e)

    async def run():
        print(f"scanning for {NAME_PREFIX}* ... (PTT style={PTT_STYLE} key={PTT_KEYCODE})")
        hit = {}
        def scan_cb(d, adv):
            name = adv.local_name or d.name or ""
            if name.startswith(NAME_PREFIX) and "dev" not in hit:
                hit["dev"] = d
        sc = BleakScanner(detection_callback=scan_cb)
        await sc.start()
        for _ in range(15):
            if "dev" in hit: break
            await asyncio.sleep(1.0)
        await sc.stop()
        dev = hit.get("dev")
        if not dev:
            print("device not found"); return
        print(f"connecting {dev.name} {dev.address}")
        while True:
            try:
                cli = BleakClient(dev)
                await cli.connect()
                await cli.start_notify(DBG_TX, on_notify)
                print("connected. Do the stick PTT gesture (tap A, then press+hold A) "
                      "and speak. Ctrl-C to quit.\n")
                while cli.is_connected:
                    await asyncio.sleep(1.0)
                print("link dropped — reconnecting...")
            except asyncio.CancelledError:
                try: await cli.disconnect()
                except Exception: pass
                return
            except Exception as e:
                print(f"  connect failed: {e}; retrying...")
                await asyncio.sleep(1.5)

    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        print("\nbye")
    finally:
        if state["sink"]:
            state["sink"].close()


if __name__ == "__main__":
    main()
