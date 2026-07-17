# Claude Code hook -> bridge daemon (UDP, fire-and-forget).
# Reads the event JSON from stdin, forwards it to the daemon, and exits
# immediately. Always exits 0 so it never blocks or breaks the session.

import json
import socket
import sys

try:
    raw = sys.stdin.read()
    data = json.loads(raw) if raw.strip() else {}
    evt = {
        "event": data.get("hook_event_name", ""),
        "session_id": data.get("session_id", ""),
        "tool_name": data.get("tool_name", ""),
        "message": data.get("message", ""),
    }
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(0.5)
    sock.sendto(json.dumps(evt).encode("utf-8"), ("127.0.0.1", 8383))
    sock.close()
except Exception:
    pass
sys.exit(0)
