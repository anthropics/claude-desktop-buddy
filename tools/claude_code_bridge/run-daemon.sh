#!/bin/bash
# Run the buddy daemon with the bundled venv Python.
# Use this instead of invoking daemon.py directly so you don't need to
# remember to activate the venv.
set -euo pipefail
cd "$(dirname "$0")"
exec .venv/bin/python daemon.py "$@"
