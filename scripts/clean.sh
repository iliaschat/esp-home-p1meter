#!/usr/bin/env bash
# Usage: scripts/clean.sh <all|public|advanced> <all|device> <board>
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
firmware="${1:?firmware: all, public or advanced}"
scope="${2:?scope: all or device}"
board="${3:?board name}"

if [[ "$firmware" == all ]]; then
  scope=all
fi
if [[ "$scope" == all ]]; then
  board=all
fi

exec bash "$root/scripts/build.sh" "$firmware" "$board" clean
