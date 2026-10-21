#!/usr/bin/env bash
# =====================================================================
#  Pelco-D Controller & Sightline SLA Build Launcher
#  Dispatches to scripts/build.sh with all supplied arguments
# =====================================================================
set -eu
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/scripts/build.sh" "$@"
