#!/bin/bash
set -euo pipefail

LOG_FILE="${1:-/tmp/jamtaba-linux-container-build.log}"

if [ ! -f "${LOG_FILE}" ]; then
    echo "Log file not found: ${LOG_FILE}" >&2
    exit 1
fi

grep -nE "error:|Error |No rule to make target|cannot find|undefined reference|file format not recognized|fatal:|make(\[[0-9]+\])?: \*\*\*" "${LOG_FILE}" | tail -80 || true
