#!/usr/bin/env python3
"""StickS3 on-device mic probe — verifies the Phase 2 ES8311 BLE audio stream.

Connects to the Claude-S3- buddy over the unencrypted debug NUS, subscribes to
notifications, and reconstructs the ADPCM audio stream emitted during a PTT
hold. Decodes (IMA ADPCM, matching src/adpcm.cpp byte-for-byte) to PCM16 and
writes a WAV so we can confirm the mic captures real speech.

This decoder is the prototype for the cc-bridge daemon's Phase 2b audio path.

Usage: hold the StickS3 PTT gesture (tap A, then press+hold A) and speak.
Release to end. The script writes one WAV per utterance and prints stats.
"""
import asyncio, base64, json, struct, sys, time, wave
from bleak import BleakScanner, BleakClient

NAME_PREFIX = "Claude-S3-"
DBG_TX = "b0c2dbe6-cc03-4000-8000-00805f9b34fb"   # stick → host NOTIFY (unencrypted)

# ---- IMA ADPCM decoder (must match src/adpcm.cpp exactly) ----
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
        out = []
        for b in data:
            out.append(self._nib(b & 0x0F))
            out.append(self._nib((b >> 4) & 0x0F))
        return out

class Session:
    def __init__(self):
        self.reset()
    def reset(self):
        self.active = False
        self.sr = 16000
        self.dec = Adpcm()
        self.pcm = []
        self.frames = 0
        self.next_seq = 0
        self.dropped = 0
        self.t0 = 0

def main_async():
    sess = Session()
    buf = bytearray()

    def write_wav(s: Session):
        if not s.pcm:
            print("  (no audio decoded)"); return
        fn = f"/Users/txie/.claude/jobs/0988cbcf/tmp/s3_utterance_{int(time.time())}.wav"
        with wave.open(fn, "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(s.sr)
            w.writeframes(struct.pack("<%dh" % len(s.pcm), *s.pcm))
        dur = len(s.pcm) / s.sr
        print(f"  WAV: {fn}")
        print(f"  frames={s.frames} dropped_seq={s.dropped} "
              f"samples={len(s.pcm)} dur={dur:.2f}s sr={s.sr}")

    def handle_line(line: str):
        line = line.strip()
        if not line or line[0] != "{":
            return
        try:
            o = json.loads(line)
        except Exception:
            return
        evt = o.get("evt")
        if evt == "ptt":
            print(f"[ptt] {o.get('state')} ts={o.get('ts')}")
        elif evt == "audio_begin":
            sess.reset()
            sess.active = True
            sess.sr = int(o.get("sr", 16000))
            sess.t0 = time.time()
            print(f"\n=== audio_begin sr={sess.sr} frame_raw={o.get('frame_raw_bytes')} "
                  f"codec={o.get('codec')} ===")
        elif evt == "audio":
            if not sess.active:
                return
            seq = int(o.get("seq", 0))
            if seq != sess.next_seq:
                gap = seq - sess.next_seq
                if gap > 0:
                    sess.dropped += gap
                    print(f"  ! seq gap: expected {sess.next_seq} got {seq} (+{gap})")
            sess.next_seq = seq + 1
            raw = base64.b64decode(o.get("data", ""))
            sess.pcm.extend(sess.dec.decode(raw))
            sess.frames += 1
            if sess.frames % 25 == 0:
                print(f"  ... {sess.frames} frames, {len(sess.pcm)} samples")
        elif evt == "audio_end":
            if not sess.active:
                return
            print(f"=== audio_end seq_total={o.get('seq_total')} reason={o.get('reason')} "
                  f"wire_p50={o.get('wire_p50')} p95={o.get('wire_p95')} p99={o.get('wire_p99')} ===")
            write_wav(sess)
            sess.active = False

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
        print(f"scanning for {NAME_PREFIX}* ...")
        hit = {}
        def scan_cb(d, adv):
            name = adv.local_name or d.name or ""
            if name.startswith(NAME_PREFIX) and "dev" not in hit:
                hit["dev"] = d
        sc = BleakScanner(detection_callback=scan_cb)
        await sc.start()
        for _ in range(12):
            if "dev" in hit: break
            await asyncio.sleep(1.0)
        await sc.stop()
        dev = hit.get("dev")
        if not dev:
            print("device not found"); return
        print(f"connecting {dev.name} {dev.address}")
        # Fresh-forget connects are flaky: the device may drop the first 1-2
        # attempts during the encrypted-NUS negotiation, like the daemon's
        # reconnect_loop tolerates. Retry connect+subscribe several times.
        for attempt in range(8):
            try:
                cli = BleakClient(dev)
                await cli.connect()
                await cli.start_notify(DBG_TX, on_notify)
                print(f"connected (attempt {attempt+1}). "
                      f"HOLD the PTT gesture and speak. Ctrl-C to quit.\n")
                try:
                    while cli.is_connected:
                        await asyncio.sleep(1.0)
                except asyncio.CancelledError:
                    await cli.disconnect()
                    return
                print("link dropped — reconnecting...")
            except Exception as e:
                print(f"  connect attempt {attempt+1} failed: {e}")
                await asyncio.sleep(1.5)
        print("gave up after retries")

    asyncio.run(run())

if __name__ == "__main__":
    try:
        main_async()
    except KeyboardInterrupt:
        print("\nbye")
