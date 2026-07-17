# Claude Code (VSCode) bridge

The official Hardware Buddy window only talks to the **Claude Desktop** app.
If you mainly use **Claude Code** (the VSCode extension) instead, this small
Python bridge gets the same experience: your device reacts to Claude Code
sessions, and you can approve or deny permission prompts with the physical
buttons.

It works by connecting Claude Code's [hooks](https://docs.claude.com/en/docs/claude-code/hooks)
to a background daemon that holds the BLE connection to the device, using
the same wire protocol described in [../../REFERENCE.md](../../REFERENCE.md).

## How it fits together

```
Claude Code hooks  --(UDP/TCP, localhost)-->  buddy_daemon.py  --(BLE)-->  device
```

- **`buddy_daemon.py`** is a persistent process. It keeps the BLE connection
  alive (auto-reconnect), listens for status events on UDP `127.0.0.1:8383`,
  and answers permission requests on TCP `127.0.0.1:8384`.
- **`buddy_hook.py`** is what Claude Code runs on `UserPromptSubmit`,
  `PostToolUse`, `Notification`, `Stop`, `SessionStart` and `SessionEnd`. It
  forwards the event JSON to the daemon over UDP and exits immediately, so it
  never blocks your session.
- **`buddy_permission_hook.py`** is what Claude Code runs on
  `PermissionRequest`. It blocks (with a generous timeout, see below) waiting
  for the daemon's TCP reply, which in turn waits for a button press on the
  device.
- **`bridge_test.py`** is a standalone script that talks to the device
  directly, without the daemon or Claude Code, useful for checking the wire
  protocol end to end.

## Setup

1. Install dependencies:

   ```bash
   pip install -r requirements.txt
   ```

2. Pair the device once, the same way you would for Claude Desktop (BLE
   bonding is stored by the OS, not by this bridge). One easy way to do the
   first pairing without installing Claude Desktop: run `bridge_test.py` once
   and complete the OS Bluetooth pairing prompt when it appears.

3. Add these hooks to your Claude Code `settings.json` (global, in
   `~/.claude/settings.json`, or per project), adjusting the paths to where
   you cloned this repo:

   ```json
   {
     "hooks": {
       "PermissionRequest": [
         {
           "hooks": [
             {
               "type": "command",
               "command": "python /path/to/bridges/claude-code-vscode/buddy_permission_hook.py",
               "timeout": 130,
               "statusMessage": "Waiting for approval on the buddy (A=yes, B=no)..."
             }
           ]
         }
       ],
       "UserPromptSubmit": [
         {"hooks": [{"type": "command", "command": "python /path/to/bridges/claude-code-vscode/buddy_hook.py", "timeout": 5, "async": true}]}
       ],
       "PostToolUse": [
         {"hooks": [{"type": "command", "command": "python /path/to/bridges/claude-code-vscode/buddy_hook.py", "timeout": 5, "async": true}]}
       ],
       "Notification": [
         {"hooks": [{"type": "command", "command": "python /path/to/bridges/claude-code-vscode/buddy_hook.py", "timeout": 5, "async": true}]}
       ],
       "Stop": [
         {"hooks": [{"type": "command", "command": "python /path/to/bridges/claude-code-vscode/buddy_hook.py", "timeout": 5, "async": true}]}
       ],
       "SessionStart": [
         {"hooks": [{"type": "command", "command": "python /path/to/bridges/claude-code-vscode/buddy_hook.py", "timeout": 5, "async": true}]}
       ],
       "SessionEnd": [
         {"hooks": [{"type": "command", "command": "python /path/to/bridges/claude-code-vscode/buddy_hook.py", "timeout": 5, "async": true}]}
       ]
     }
   }
   ```

   The `timeout` on `PermissionRequest` must stay a few seconds above
   `PERM_WAIT` in `buddy_daemon.py` (120s by default), or Claude Code will
   cut the hook off before the daemon has a chance to reply.

4. Run `buddy_daemon.py` in the background, keeping it alive across reboots.
   On Windows, a Scheduled Task running at logon works well:

   ```powershell
   $action = New-ScheduledTaskAction -Execute "pythonw.exe" -Argument '"C:\path\to\buddy_daemon.py"'
   $trigger = New-ScheduledTaskTrigger -AtLogOn
   Register-ScheduledTask -TaskName "ClaudeBuddyBridge" -Action $action -Trigger $trigger -RunLevel Highest
   ```

   On macOS/Linux, a `launchd`/`systemd --user` unit running
   `python buddy_daemon.py` achieves the same thing.

5. **Claude Desktop must stay closed** while the bridge is running. BLE only
   accepts one central connection at a time, and Claude Desktop's own bridge
   reconnects on its own in the background, which steals the connection.

## Configuration

- `BUDDY_OWNER` environment variable: name shown as the device owner
  (defaults to `"you"`).
- `PERM_WAIT` in `buddy_daemon.py`: seconds to wait for a button press before
  falling back to the normal in-editor prompt. Defaults to 120s; the original
  15s default felt too rushed for anything you actually want to read before
  approving.
- By default `buddy_permission_hook.py` routes every tool call to the
  device's buttons. If you'd rather keep risky tools (Bash, PowerShell,
  Write, Edit, ...) on the normal in-editor prompt, where the full command is
  visible, and only delegate read-only tools (Read, Glob, Grep, ...) to the
  button, add a filter at the top of `buddy_permission_hook.py` before it
  builds the request. See "Known issues" below for why this matters.

## Known issues

- **The device's screen only fits ~42 characters of the command** (`hint` is
  truncated in `promptHint`, see `data.h` and the wrapping in
  `main.cpp`). Approving a long shell command from the button means you
  cannot see the end of it. If you tend to approve without reading closely,
  consider the tool filter mentioned above, or keep Bash/PowerShell on the
  normal VSCode prompt.
- **The device sometimes sends two BLE notifications back to back without a
  newline between them** (for example after a fast double press). The
  daemon's notification handler originally split strictly on `\n`, so a
  glued pair of JSON objects failed to parse and the real decision was
  silently dropped, looking like the button press "didn't register." This
  bridge works around it in `on_notify`/`_parse_objects` by extracting as
  many JSON objects as are present in a buffer, with or without a separator,
  instead of assuming exactly one object per line.
- Some third-party prebuilt firmware packages for other boards were found to
  reuse a **fixed/hardcoded BLE address** instead of deriving it from the
  chip's real MAC. If you run two devices flashed from the same prebuilt
  package, they may advertise under the same address and the bridge (or any
  BLE central) won't be able to tell them apart. Only run one such device at
  a time, or check the firmware source for a hardcoded address if you build
  it yourself.
