#!/usr/bin/env python3
"""Claude Code hook → buddy-daemon adapter.

Configured as a hook command in `.claude/settings.json`. Reads the Claude
Code hook payload from stdin and forwards a fire-and-forget message to the
daemon over `/tmp/claude-buddy.sock`.

This bridge is mirror-only: it does NOT intercept permission decisions.
Claude Code's own `permissions` config (allow / ask / deny) decides what
gets prompted; when the CLI needs the user's attention it fires a
`Notification` hook, which we forward to the device for a vibrate +
on-screen message. No PreToolUse interception, no separate policy file.

If the daemon is unreachable (not running, device disconnected) the hook
exits silently. A dead daemon never blocks your terminal.
"""
from __future__ import annotations

import json
import os
import socket
import sys

SOCK_PATH = os.environ.get("CLAUDE_BUDDY_SOCK", "/tmp/claude-buddy.sock")


def send(payload: dict, timeout: float = 2.0) -> None:
    """Fire-and-forget send to the daemon. Silent on failure."""
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect(SOCK_PATH)
        s.sendall((json.dumps(payload, ensure_ascii=False) + "\n").encode("utf-8"))
        s.close()
    except (FileNotFoundError, ConnectionRefusedError, socket.timeout, OSError) as e:
        print(f"[buddy-hook] daemon unreachable: {e}", file=sys.stderr)
    except Exception as e:
        print(f"[buddy-hook] unexpected error: {e}", file=sys.stderr)


def main() -> None:
    try:
        evt = json.load(sys.stdin)
    except Exception:
        sys.exit(0)

    name = evt.get("hook_event_name", "")
    sid = evt.get("session_id", "")

    if name == "SessionStart":
        send({"event": "SessionStart", "session_id": sid})
    elif name == "PostToolUse":
        send({"event": "PostToolUse",
              "tool": evt.get("tool_name", ""),
              "session_id": sid})
    elif name == "Stop":
        send({"event": "Stop",
              "session_id": sid,
              "transcript_path": evt.get("transcript_path", "")})
    elif name == "SessionEnd":
        send({"event": "SessionEnd", "session_id": sid})
    elif name == "Notification":
        send({"event": "Notification",
              "msg": (evt.get("message") or "attention")[:23]})

    sys.exit(0)


if __name__ == "__main__":
    main()
