#!/usr/bin/env bash
# Hosts an OpenBW game that a Java bot (Atlantis, JBWAPI client) can attach to.
#
# Usage:
#   Terminal 1: ./scripts/run-openbw-server.sh [map] [race]
#   Terminal 2: cd /ravaelles/JAVA/starcraft-ai/Atlantis && java -jar Atlantis.jar
#               (with bwapi-data/AI/ENV containing GAME_LAUNCHER=OPENBW)
#
# The script must run with the working directory build/test: MPQs
# (StarDat.mpq, BrooDat.mpq, Patch_rt.mpq), maps/ and bwapi-data/ live there.
# BWAPI settings can be overridden through BWAPI_CONFIG_* environment
# variables instead of editing bwapi.ini (see openbw/bwapi README).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT/build/test"

MAP="${1:-${BWAPI_CONFIG_AUTO_MENU__MAP:-maps/sscai/(4)Python.scx}}"
RACE="${2:-${BWAPI_CONFIG_AUTO_MENU__RACE:-Protoss}}"

if [[ ! -f "$MAP" ]]; then
    echo "Map not found: $MAP" >&2
    exit 1
fi
for mpq in StarDat.mpq BrooDat.mpq Patch_rt.mpq; do
    if [[ ! -f "$mpq" ]]; then
        echo "Missing $mpq in $PWD (see DOCS/HOW-TESTS-WORK.md)" >&2
        exit 1
    fi
done

echo "Hosting OpenBW game: map=$MAP race=$RACE"
echo "Start the Java client now (it polls until the server is up)."
export BWAPI_CONFIG_AUTO_MENU__MAP="$MAP"
export BWAPI_CONFIG_AUTO_MENU__RACE="$RACE"
exec "$ROOT/build/bin/BWAPILauncher"
