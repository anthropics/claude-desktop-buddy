# Minimal end-to-end test of the BLE bridge protocol, independent of
# the daemon and Claude Code. Connects to the device, sends a time sync
# and owner name, then walks through busy -> attention -> idle so you
# can confirm the wire protocol works before wiring up hooks.
#
# Requirement: Claude Desktop must be closed (its own bridge competes
# for the same BLE connection).
#
# Usage: python bridge_test.py

import asyncio
import json
import os
import sys
import time

from bleak import BleakClient, BleakScanner

NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # write here (PC -> device)
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # notifications (device -> PC)

OWNER = os.environ.get("BUDDY_OWNER", "you")


def snapshot(total, running, waiting, msg, entries=None, prompt=None):
    snap = {
        "total": total,
        "running": running,
        "waiting": waiting,
        "msg": msg,
        "entries": entries or [],
        "tokens": 0,
        "tokens_today": 0,
    }
    if prompt:
        snap["prompt"] = prompt
    return snap


async def send(client, obj):
    line = (json.dumps(obj) + "\n").encode("utf-8")
    # split at the MTU boundary; the firmware reassembles up to \n
    chunk = 180
    for i in range(0, len(line), chunk):
        await client.write_gatt_char(NUS_RX, line[i : i + chunk], response=True)
    print(f">> {json.dumps(obj)[:100]}")


def on_notify(_, data: bytearray):
    try:
        print(f"<< {data.decode('utf-8').strip()}")
    except UnicodeDecodeError:
        print(f"<< (bytes) {data.hex()}")


async def main():
    print("Scanning for a Claude-* device ...")
    device = None
    for attempt in range(3):
        devices = await BleakScanner.discover(timeout=8.0)
        candidates = [d for d in devices if d.name and d.name.startswith("Claude")]
        if candidates:
            device = candidates[0]
            break
        print(f"  nothing yet (attempt {attempt + 1}/3) -- is the device awake?")
    if not device:
        print("Not found. Check: device powered on/awake, Claude Desktop closed.")
        sys.exit(1)

    print(f"Found: {device.name} [{device.address}] -- connecting...")
    async with BleakClient(device) as client:
        print("Connected. Subscribing to notifications...")
        await client.start_notify(NUS_TX, on_notify)

        # one-shot connection setup: time + owner
        offset = -time.timezone if not time.localtime().tm_isdst else -time.altzone
        await send(client, {"time": [int(time.time()), offset]})
        await send(client, {"cmd": "owner", "name": OWNER})
        await asyncio.sleep(1)

        print("\n--- BUSY (should look 'working/sweating') for 8s ---")
        await send(client, snapshot(1, 1, 0, "Claude Code: running...",
                                    ["12:00 testing the BLE bridge"]))
        await asyncio.sleep(8)

        print("\n--- ATTENTION (should ask for approval, LED blinks) ---")
        print("    Button A = approve | Button B = deny (watch the << reply)")
        await send(client, snapshot(1, 0, 1, "approve: Bash",
                                    ["12:00 rm -rf /tmp/test"],
                                    prompt={"id": "req_test1", "tool": "Bash",
                                            "hint": "echo bridge test"}))
        await asyncio.sleep(15)

        print("\n--- IDLE (should relax) for 5s ---")
        await send(client, snapshot(1, 0, 0, "all quiet"))
        await asyncio.sleep(5)

        print("\n--- DONE: zero sessions (will fall asleep after a while) ---")
        await send(client, snapshot(0, 0, 0, ""))
        await asyncio.sleep(2)

        await client.stop_notify(NUS_TX)
    print("Disconnected. Test complete.")


if __name__ == "__main__":
    asyncio.run(main())
