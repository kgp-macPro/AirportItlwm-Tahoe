#!/bin/bash
# Desktop tool setup only. Does not install a driver or change the network.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
command -v python3 >/dev/null || { echo 'STOP: Python 3.9+ is required; no dependencies installed automatically.'; exit 1; }
python3 -c 'import sys; assert sys.version_info >= (3,9), "Python 3.9+ required"'
python3 "$HERE/install_tools.py" --package "$HERE"
if [[ "${OPEN_INSTRUCTIONS:-1}" == 1 && -x /usr/bin/open ]]; then
  /usr/bin/open "$HOME/Desktop/AirportItlwm/TOOLS/README.md"
fi
