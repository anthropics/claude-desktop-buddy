# Claude Code → Hardware Buddy bridge

Drives a `Claude-*` BLE device from Claude Code CLI directly, without the
Claude desktop app. Mirrors Claude Code's own permission flow: when the
CLI fires a `Notification` (i.e. it needs your attention — typically a
permission prompt for something on your `permissions.ask` list), the
device buzzes and shows the message. Other events drive the device's
status display: idle / busy / token counter / celebrate animations.

This bridge is **mirror-only**. It does not intercept or override
permission decisions — Claude Code's `permissions` config (allow / ask /
deny) remains the single source of truth. You don't need a separate
buddy policy.

## Architecture

```
Claude Code (terminal) ─▶ hook.py ─▶ Unix socket ─▶ daemon.py ─▶ BLE ─▶ M5StickC
```

- **`daemon.py`** is a long-running process. Maintains the BLE connection,
  accumulates session state, pushes snapshots to the device.
- **`hook.py`** is a short-lived script. Claude Code spawns one per hook
  event; it forwards a fire-and-forget message to the daemon over
  `/tmp/claude-buddy.sock`. No PreToolUse interception, no decision
  round-trip — the hook always exits in a couple of milliseconds.

## Setup

### 1. Install bleak (in a venv inside this directory)

```bash
cd tools/claude_code_bridge
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

The daemon is launched via `run-daemon.sh` which uses this venv's Python.
The hook script doesn't need bleak — only stdlib — so it'll run with
whatever Python is on `PATH`.

### 2. Pair the device with macOS (one time)

The device firmware uses LE Secure Connections bonding, so the first time
the daemon connects, macOS will pop a dialog asking for the 6-digit
passkey shown on the M5StickC screen. Enter it. After that, reconnects
are silent.

If the dialog never appears: open **System Settings → Bluetooth**, find
the `Claude-XXXX` device, click **Connect**, and macOS will prompt for the
passkey there.

### 3. Configure the hook

Edit `~/.claude/settings.json` (or your project's `.claude/settings.json`)
and merge the following into your `hooks` block. Use absolute paths;
adjust if you cloned elsewhere:

```json
{
  "hooks": {
    "PostToolUse": [
      { "hooks": [{ "type": "command", "command": "/path/to/claude-desktop-buddy/tools/claude_code_bridge/hook.py" }] }
    ],
    "SessionStart": [
      { "hooks": [{ "type": "command", "command": "/path/to/claude-desktop-buddy/tools/claude_code_bridge/hook.py" }] }
    ],
    "SessionEnd": [
      { "hooks": [{ "type": "command", "command": "/path/to/claude-desktop-buddy/tools/claude_code_bridge/hook.py" }] }
    ],
    "Stop": [
      { "hooks": [{ "type": "command", "command": "/path/to/claude-desktop-buddy/tools/claude_code_bridge/hook.py" }] }
    ],
    "Notification": [
      { "hooks": [{ "type": "command", "command": "/path/to/claude-desktop-buddy/tools/claude_code_bridge/hook.py" }] }
    ]
  }
}
```

That's it — no `PreToolUse`, no separate policy file.

### 4. (Optional) Tune which tools fire `Notification`

Buddy buzzes whenever Claude Code itself decides to ask you. So tuning
buddy = tuning Claude Code. In your `~/.claude/settings.json`, under
`permissions`:

- Things you **never** want buddy to bother you about → put in `allow`
- Things you **do** want a physical heads-up on → put in `ask`
- Things you want blocked outright → put in `deny`

Example: with `defaultMode: "bypassPermissions"`, only the items in
`ask` produce a Notification, so buddy stays quiet except for the few
high-stakes operations you flagged. Typical `ask` list:

```json
{
  "permissions": {
    "ask": [
      "Bash(git push:*)",
      "Bash(npm publish:*)",
      "Bash(kubectl apply:*)",
      "Bash(kubectl delete:*)"
    ],
    "defaultMode": "bypassPermissions"
  }
}
```

## Running

### Start the daemon

```bash
cd tools/claude_code_bridge
./run-daemon.sh
```

You should see something like:

```
[12:34:01] scanning for Claude-* device...
[12:34:03] found Claude-A4B7 (...), connecting...
[12:34:04] connected, mtu=185
[12:34:04] hook server listening on /tmp/claude-buddy.sock
```

The device will switch from sleep to idle.

Leave this running in a terminal (or backgrounded with `&`,
`tmux`, `screen`, etc). When you're done with the device,
Ctrl+C the daemon — the device will fall back to sleep.

### Use Claude Code as normal

In another terminal:

```bash
claude
```

Now ask it to do something on your `permissions.ask` list, e.g.
*"git push this branch"*. The device should:

1. Briefly show the notification message in its status line.
2. Trigger its "completed" animation (buzz / celebrate).

Approve or deny in the terminal as usual. The buddy is a heads-up only,
not a remote control.

## Troubleshooting

### Device not found during scan

- Make sure you've recently woken it (any button press) — the M5StickC's
  BLE radio sleeps with the device.
- Check `Settings → Bluetooth → ON` on the stick.
- If you previously paired it with the Claude desktop app and want to
  re-pair: long-hold A → settings → reset → factory reset.

### "daemon unreachable" in hook stderr

- Daemon isn't running, or socket path mismatch. Confirm `/tmp/claude-buddy.sock`
  exists when the daemon is up.
- Set `CLAUDE_BUDDY_SOCK=/your/path` in both daemon and hook env if you've
  customized.

### Permission dialog from macOS keeps appearing

The first connection after a factory reset on the device will re-prompt.
After that, macOS stores the bond. If you're hitting it repeatedly,
something's wrong with the bond — try removing the device from
**System Settings → Bluetooth** and pairing fresh.

### Hooks not firing at all

```bash
claude --debug
```

This prints hook execution. If the hook script exits with a Python
traceback, fix and retry. If hooks aren't being invoked at all, check
your `~/.claude/settings.json` syntax with `python -c 'import json; json.load(open("$HOME/.claude/settings.json"))'`.

### Buddy never buzzes even when CLI asks

The bridge only buzzes on the `Notification` event. If `defaultMode` is
`bypassPermissions` and the tool isn't on your `ask` list, the CLI
auto-approves silently and never fires `Notification` — buddy will stay
quiet by design. Move the tool into `permissions.ask` to opt in.

## Notes

- The daemon doesn't persist tokens across restarts. The cumulative count
  resets each time you restart the daemon. The device's NVS-backed
  `tokens_today` (which drives the celebrate animation) tracks deltas
  from the bridge, so you'll get celebrations on each 50K chunk regardless
  of daemon restarts.
- For multiple Claude Code sessions running concurrently, the device shows
  aggregate state (total sessions). Per-session detail isn't displayed.
- Older firmware that still tries to send `{"cmd":"permission",...}`
  responses is fine — the daemon ignores them as stale, since this bridge
  no longer pushes prompts.
