# Bridge daemon: Claude Code (VSCode) -> Buddy device.
#
# - Keeps a persistent BLE connection to the buddy (auto-reconnect).
# - UDP 127.0.0.1:8383: fire-and-forget status events from hooks.
# - TCP 127.0.0.1:8384: blocking permission requests (PermissionRequest hook).
#   The device shows the prompt; button A approves, B denies; no answer
#   within PERM_WAIT seconds returns "ask" and the normal VSCode prompt
#   appears instead.
#
# Requirement: Claude Desktop must be closed while this runs (BLE only
# accepts one central at a time; Claude Desktop's own bridge reconnects
# on its own and would steal the connection).
# Intended to run hidden at logon (see README.md for the Windows
# Scheduled Task setup); logs to daemon.log next to this file.

import asyncio
import json
import os
import time
from datetime import datetime

from bleak import BleakClient, BleakScanner

NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

UDP_HOST, UDP_PORT = "127.0.0.1", 8383
TCP_HOST, TCP_PORT = "127.0.0.1", 8384
OWNER = os.environ.get("BUDDY_OWNER", "you")
SESSION_TTL = 30 * 60      # drop a session after 30 min of inactivity
KEEPALIVE = 10             # firmware considers the link dead after ~30s without a snapshot
PERM_WAIT = 120            # seconds to wait for a button press before falling back to the normal prompt

LOG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "daemon.log")

sessions: dict[str, dict] = {}
dirty = asyncio.Event()
ble_connected = False

# pending permission prompt (one at a time; a second request falls back
# to the normal VSCode prompt while one is already pending)
pending_id: str | None = None
pending_future: asyncio.Future | None = None
pending_prompt: dict | None = None
_perm_seq = 0


def log(msg: str):
    line = f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')} {msg}"
    print(line, flush=True)
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except OSError:
        pass


def rotate_log():
    try:
        if os.path.exists(LOG_PATH) and os.path.getsize(LOG_PATH) > 1_000_000:
            os.replace(LOG_PATH, LOG_PATH + ".old")
    except OSError:
        pass


def now_hhmm() -> str:
    return datetime.now().strftime("%H:%M")


def prune():
    cutoff = time.time() - SESSION_TTL
    for sid in [s for s, v in sessions.items() if v["ts"] < cutoff]:
        del sessions[sid]


def build_snapshot() -> dict:
    prune()
    total = len(sessions)
    running = sum(1 for v in sessions.values() if v["state"] == "running")
    waiting = sum(1 for v in sessions.values() if v["state"] == "waiting")

    msg = ""
    entries: list[str] = []
    for v in sorted(sessions.values(), key=lambda v: -v["ts"]):
        entries.extend(v["entries"][:2])
        if not msg and v["state"] == "waiting":
            msg = v.get("msg") or "approval pending"
    if not msg and running:
        msg = "working..."

    snap = {
        "total": total,
        "running": running,
        "waiting": waiting if not pending_prompt else max(waiting, 1),
        "msg": msg[:48],
        "entries": entries[:4],
        "tokens": 0,
        "tokens_today": 0,
    }
    if pending_prompt:
        snap["prompt"] = pending_prompt
        snap["msg"] = f"approve: {pending_prompt.get('tool', '?')}"[:48]
    return snap


def handle_event(evt: dict):
    sid = evt.get("session_id") or "default"
    name = evt.get("event", "")
    s = sessions.setdefault(sid, {"state": "idle", "ts": 0, "entries": [], "msg": ""})
    s["ts"] = time.time()

    if name == "UserPromptSubmit":
        s["state"] = "running"
        s["msg"] = ""
    elif name in ("PreToolUse", "PostToolUse"):
        s["state"] = "running"
        tool = evt.get("tool_name") or "?"
        entry = f"{now_hhmm()} {tool}"
        if not s["entries"] or s["entries"][0] != entry:
            s["entries"].insert(0, entry)
            s["entries"] = s["entries"][:4]
    elif name == "Notification":
        text = (evt.get("message") or "").lower()
        if "permission" in text or "approve" in text:
            s["state"] = "waiting"
            s["msg"] = evt.get("message", "")[:48]
        elif "waiting" in text or "input" in text:
            s["state"] = "idle"
    elif name in ("Stop", "SubagentStop"):
        s["state"] = "idle"
        s["msg"] = ""
    elif name == "SessionEnd":
        sessions.pop(sid, None)
    elif name == "SessionStart":
        s["state"] = "idle"

    dirty.set()


class HookProtocol(asyncio.DatagramProtocol):
    def datagram_received(self, data, addr):
        try:
            evt = json.loads(data.decode("utf-8"))
            log(f"[hook] {evt.get('event')} sid={str(evt.get('session_id'))[:8]}")
            handle_event(evt)
        except Exception as e:
            log(f"[hook] invalid event: {e}")


async def handle_permission_request(reader, writer):
    """The PermissionRequest hook connects here and waits for the device's decision."""
    global pending_id, pending_future, pending_prompt, _perm_seq
    decision = "ask"
    try:
        raw = await asyncio.wait_for(reader.readline(), timeout=3)
        req = json.loads(raw.decode("utf-8"))
        tool = req.get("tool_name") or "?"
        hint = (req.get("hint") or "")[:64]

        if not ble_connected:
            log(f"[perm] {tool}: device disconnected -> ask")
        elif pending_id is not None:
            log(f"[perm] {tool}: a prompt is already pending -> ask")
        else:
            _perm_seq += 1
            pending_id = f"req_cc_{_perm_seq}"
            pending_prompt = {"id": pending_id, "tool": tool, "hint": hint}
            pending_future = asyncio.get_running_loop().create_future()
            dirty.set()  # push the prompt snapshot to the device right away
            log(f"[perm] {tool} ({hint!r}) -> waiting for a button press for {PERM_WAIT}s")
            try:
                decision = await asyncio.wait_for(pending_future, timeout=PERM_WAIT)
                log(f"[perm] device answered: {decision}")
            except asyncio.TimeoutError:
                log("[perm] timeout -> ask (normal prompt in VSCode)")
            finally:
                pending_id = None
                pending_prompt = None
                pending_future = None
                dirty.set()  # clear the prompt on the device
    except Exception as e:
        log(f"[perm] error: {e}")

    try:
        writer.write((json.dumps({"decision": decision}) + "\n").encode("utf-8"))
        await writer.drain()
        writer.close()
    except Exception:
        pass


_rx_buf = b""
_json_decoder = json.JSONDecoder()


def _parse_objects(text: str):
    """Extract one or more JSON objects from a string, even when the
    firmware sends two notifications back to back without a newline
    between them (observed in practice; see README.md 'Known issues')."""
    objs = []
    i, n = 0, len(text)
    while i < n:
        while i < n and text[i].isspace():
            i += 1
        if i >= n:
            break
        try:
            obj, end = _json_decoder.raw_decode(text, i)
        except json.JSONDecodeError:
            break
        objs.append(obj)
        i = end
    return objs


def on_notify(_, data: bytearray):
    """Notifications from the device: acks and permission decisions (JSON per line)."""
    global _rx_buf
    _rx_buf += bytes(data)
    while b"\n" in _rx_buf:
        line, _rx_buf = _rx_buf.split(b"\n", 1)
        text = line.decode("utf-8", errors="replace").strip()
        if not text:
            continue
        log(f"[device] {text}")
        for obj in _parse_objects(text):
            if obj.get("cmd") == "permission":
                if pending_future and not pending_future.done() and obj.get("id") == pending_id:
                    d = "allow" if obj.get("decision") == "once" else "deny"
                    pending_future.set_result(d)


async def ble_send(client, obj):
    line = (json.dumps(obj) + "\n").encode("utf-8")
    for i in range(0, len(line), 180):
        await client.write_gatt_char(NUS_RX, line[i : i + 180], response=True)


async def run_connection():
    global ble_connected
    log("[ble] scanning for Claude-* ...")
    devices = await BleakScanner.discover(timeout=8.0)
    dev = next((d for d in devices if d.name and d.name.startswith("Claude")), None)
    if not dev:
        raise RuntimeError("device not found")

    log(f"[ble] connecting to {dev.name} [{dev.address}]")
    async with BleakClient(dev) as client:
        await client.start_notify(NUS_TX, on_notify)
        offset = -time.timezone if not time.localtime().tm_isdst else -time.altzone
        await ble_send(client, {"time": [int(time.time()), offset]})
        await ble_send(client, {"cmd": "owner", "name": OWNER})
        ble_connected = True
        log("[ble] connected and synced")

        try:
            while client.is_connected:
                try:
                    await asyncio.wait_for(dirty.wait(), timeout=KEEPALIVE)
                except asyncio.TimeoutError:
                    pass
                dirty.clear()
                await ble_send(client, build_snapshot())
        finally:
            ble_connected = False


async def main():
    rotate_log()
    log(f"=== buddy daemon starting (pid {os.getpid()}) ===")
    loop = asyncio.get_running_loop()

    transport, _ = await loop.create_datagram_endpoint(
        HookProtocol, local_addr=(UDP_HOST, UDP_PORT)
    )
    log(f"[udp] hook events on {UDP_HOST}:{UDP_PORT}")

    server = await asyncio.start_server(handle_permission_request, TCP_HOST, TCP_PORT)
    log(f"[tcp] permission requests on {TCP_HOST}:{TCP_PORT}")

    try:
        while True:
            try:
                await run_connection()
                log("[ble] disconnected; retrying in 5s")
            except Exception as e:
                log(f"[ble] {e}; retrying in 5s")
            await asyncio.sleep(5)
    finally:
        transport.close()
        server.close()


if __name__ == "__main__":
    asyncio.run(main())
