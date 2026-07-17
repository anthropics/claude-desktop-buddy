# Claude Code PermissionRequest hook -> bridge daemon (blocking TCP).
# Asks the device whether to approve the tool call. Button A = allow,
# button B = deny. No answer (timeout, daemon down, device off) exits
# with no output, and Claude Code's normal permission prompt appears
# as usual.
#
# By default every tool is routed to the device. If you would rather
# keep risky tools (Bash, PowerShell, Write, Edit, ...) on the normal
# in-editor prompt -- where you can read the full command before
# approving -- and only delegate read-only tools to the button, filter
# on `tool` before building `req` below. See README.md "Known issues"
# for why: the device's screen only fits ~42 characters of the command,
# so approving long shell commands from the button is effectively blind.

import json
import socket
import sys


def hint_from(tool, tool_input):
    if not isinstance(tool_input, dict):
        return ""
    for key in ("command", "file_path", "url", "prompt", "pattern"):
        v = tool_input.get(key)
        if isinstance(v, str) and v:
            return v[:64]
    return ""


try:
    data = json.loads(sys.stdin.read() or "{}")
    tool = data.get("tool_name", "?")
    req = {
        "tool_name": tool,
        "hint": hint_from(tool, data.get("tool_input")),
        "session_id": data.get("session_id", ""),
    }

    sock = socket.create_connection(("127.0.0.1", 8384), timeout=1.0)
    sock.settimeout(125)  # daemon waits PERM_WAIT=120s; 5s margin
    sock.sendall((json.dumps(req) + "\n").encode("utf-8"))
    buf = b""
    while b"\n" not in buf:
        chunk = sock.recv(1024)
        if not chunk:
            break
        buf += chunk
    sock.close()

    decision = json.loads(buf.decode("utf-8")).get("decision", "ask")
    if decision in ("allow", "deny"):
        print(json.dumps({
            "hookSpecificOutput": {
                "hookEventName": "PermissionRequest",
                "decision": {"behavior": decision},
            }
        }))
except Exception:
    pass  # any failure -> no output -> normal prompt
sys.exit(0)
