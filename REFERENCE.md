# Hardware Buddy BLE Protocol

This is the wire protocol the Claude desktop apps speak over Bluetooth LE.
You don't need anything from this repository to implement it. Any device
that can advertise the Nordic UART Service and parse newline-delimited JSON
will work: Arduino, ESP32, nRF52, a Raspberry Pi with a BLE dongle.

## Enabling the bridge

The BLE bridge is off by default. In Claude for macOS or Windows:

1. **Help → Troubleshooting → Enable Developer Mode** — adds a **Developer**
   menu to the menu bar.
2. **Developer → Open Hardware Buddy…** — opens the pairing window.
3. Click **Connect** and pick your device from the scan list. The OS will
   prompt for Bluetooth permission on first use.

Once paired the bridge auto-reconnects in the background; you only need the
window open for initial pairing, the stats panel, or the folder drop target.

## Transport

**BLE Nordic UART Service** (the de-facto serial-over-BLE standard):

|                               | UUID                                   |
| ----------------------------- | -------------------------------------- |
| Service                       | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| RX (desktop → device, write)  | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| TX (device → desktop, notify) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

Advertise a name starting with `Claude` over the Nordic UART Service so the
device picker can filter to you. Appending a few bytes of your BT MAC keeps
multiple devices distinguishable in the picker.

Everything on the wire is UTF-8 JSON—one object per line, terminated with
`\n`. The desktop reassembles multi-packet lines on its end (notifications
fragment at the MTU boundary, just send the bytes). Your device needs to do
the same: accumulate bytes until you see `\n`, then parse.

## Heartbeat snapshot

The desktop apps send a heartbeat snapshot whenever something changes, plus
a keepalive every 10 seconds:

```json
{
  "total": 3,
  "running": 1,
  "waiting": 1,
  "msg": "approve: Bash",
  "entries": ["10:42 git push", "10:41 yarn test", "10:39 reading file..."],
  "tokens": 184502,
  "tokens_today": 31200,
  "prompt": {
    "id": "req_abc123",
    "tool": "Bash",
    "hint": "rm -rf /tmp/foo"
  }
}
```

| Field          | Meaning                                                                           |
| -------------- | --------------------------------------------------------------------------------- |
| `total`        | Count of all sessions                                                             |
| `running`      | Sessions actively generating                                                      |
| `waiting`      | Sessions blocked on a permission prompt                                           |
| `msg`          | One-line summary suitable for a small display                                     |
| `entries`      | Recent transcript lines, newest first (capped to a few)                           |
| `tokens`       | Cumulative output tokens since the desktop app started                            |
| `tokens_today` | Output tokens since local midnight (persisted, survives restart)                  |
| `prompt`       | Only present when a permission decision is needed. The `id` is what you echo back |

A few useful derived signals: `running > 0` means at least one session is
actively generating, `waiting > 0` means a permission prompt is blocking,
and `total == 0` means nothing is open. `tokens_today` resets at local
midnight if you want a daily counter.

If you don't receive a snapshot for ~30 seconds, treat the connection as
dead.

## Turn events

Each completed turn also fires a one-shot event containing the raw SDK
content array—text blocks, tool calls, and any other content from the
message. Events that serialize larger than 4KB are dropped (measured in
UTF-8 bytes, not character count).

```json
{
  "evt": "turn",
  "role": "assistant",
  "content": [{ "type": "text", "text": "..." }]
}
```

## Permission decisions

When `prompt` is present, your device can return a response. Send one of:

```json
{"cmd":"permission","id":"req_abc123","decision":"once"}
{"cmd":"permission","id":"req_abc123","decision":"deny"}
```

The `id` must match `prompt.id` exactly. The desktop forwards this to the
session manager: `"once"` approves the tool call, `"deny"` rejects it.

## One-shot on connect

Time sync (epoch seconds + timezone offset in seconds):

```json
{ "time": [1775731234, -25200] }
```

Owner name (the user's first name from their account):

```json
{ "cmd": "owner", "name": "Felix" }
```

## Commands and acks

Any command the desktop sends with a `cmd` field expects a matching ack:

```json
{ "ack": "<same as cmd>", "ok": true, "n": 0 }
```

Set `ok:false` and optionally `error:"..."` if you couldn't do it. `n` is a
generic counter (e.g. bytes written for chunk acks, otherwise 0).

| Command                          | Payload                  | Ack you send back            |
| -------------------------------- | ------------------------ | ---------------------------- |
| `{"cmd":"status"}`               | —                        | see Status response below    |
| `{"cmd":"name","name":"Clawd"}`  | sets device display name | `{"ack":"name","ok":true}`   |
| `{"cmd":"owner","name":"Felix"}` | sets owner name          | `{"ack":"owner","ok":true}`  |
| `{"cmd":"unpair"}`               | erase stored BLE bonds   | `{"ack":"unpair","ok":true}` |

**Status response.** The desktop polls this every couple of seconds to
populate the Hardware Buddy window's stats panel:

```json
{
  "ack": "status",
  "ok": true,
  "data": {
    "name": "Clawd",
    "sec": true,
    "bat": { "pct": 87, "mV": 4012, "mA": -120, "usb": true },
    "sys": { "up": 8412, "heap": 84200 },
    "stats": { "appr": 42, "deny": 3, "vel": 8, "nap": 12, "lvl": 5 }
  }
}
```

You can omit fields you don't have. `bat.mA` negative means charging.

## Folder push

The Hardware Buddy window has a drop target. Dropping a folder there streams
its flat contents to your device. The transport is content-agnostic: GIFs,
config blobs, firmware images, whatever you want under 1.8MB total.

```
desktop:  {"cmd":"char_begin","name":"bufo","total":184320}
device:   {"ack":"char_begin","ok":true}

desktop:  {"cmd":"file","path":"manifest.json","size":412}
device:   {"ack":"file","ok":true}
desktop:  {"cmd":"chunk","d":"<base64>"}
device:   {"ack":"chunk","ok":true,"n":<bytes_written_so_far>}
          ...repeat chunk until file is done...
desktop:  {"cmd":"file_end"}
device:   {"ack":"file_end","ok":true,"n":<final_size>}

          ...repeat file/chunk/file_end for each file...

desktop:  {"cmd":"char_end"}
device:   {"ack":"char_end","ok":true}
```

The desktop sends every regular file in the folder (no recursion, dotfiles
skipped), base64-encodes each chunk, and waits for each ack before sending
the next. You decode and append; the protocol is sequential so you don't
need to buffer whole files.

`char_begin.name` is whatever the folder is called, unless the folder
contains a `manifest.json` with a `"name"` field, in which case that wins.

If your device doesn't want pushed files, don't ack `char_begin`. The
desktop times out after a few seconds and tells the user it failed.

## Security and pairing

The desktop app connects whether or not your device requests link
encryption, but transcript snippets and tool-call hints flow over this
link, so an unencrypted device is sniffable by anyone in radio range
with a cheap nRF dongle. You should require **LE Secure Connections
bonding**: mark your NUS characteristics (and the TX CCCD) as
encrypted-only and advertise DisplayOnly IO capability. The first GATT
access then triggers OS pairing — the desktop prompts the user for the
6-digit passkey your device displays — and the link is AES-CCM-encrypted
from then on. Reconnects reuse the stored LTK without re-prompting.

The desktop app supports both encrypted and unencrypted devices. Two
protocol hooks tie into pairing:

- Include `"sec": true` in your status ack's `data` once the link is
  encrypted (or `false`/omit it if you don't bond).
- Handle `{"cmd":"unpair"}` by erasing your stored bonds. The desktop
  sends this when the user clicks **Forget**, so the next pairing shows
  a fresh passkey. Ack it like any other command.

If you accept the folder-push protocol, validate `file.path` before
writing — the desktop sends whatever filenames are in the dropped
folder, so reject `..` and absolute paths unless your filesystem holds
nothing you'd mind overwritten.

## Availability

The BLE API is only available when the desktop apps are in developer mode
(**Help → Troubleshooting → Enable Developer Mode**). It's intended for
makers and developers and isn't an officially supported product feature.
