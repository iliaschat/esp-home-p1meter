#!/usr/bin/env bash
# Usage: scripts/build.sh <public|advanced|all> <device|all> [action]
#    or: scripts/build.sh <firmware>/<device|all> [action]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
firmware="${1:?firmware: public, advanced or all}"
if [[ "$firmware" == */* ]]; then
  device="${firmware#*/}"
  firmware="${firmware%%/*}"
  action="${2:-compile}"
else
  device="${2:?device: a board name or all}"
  action="${3:-compile}"
fi
all_devices=(wemos-d1-mini nodemcuv2 esp32-devkit esp32-s2 esp32-s3 esp32-c3)

case "$action" in
  config|compile|run|logs|clean) ;;
  *) echo "Unknown action: $action" >&2; exit 1 ;;
esac

if [[ "$firmware" == all || "$device" == all ]]; then
  if [[ "$action" != clean ]]; then
    echo "'all' is only supported for clean" >&2
    exit 1
  fi
fi

firmwares=("$firmware")
if [[ "$firmware" == all ]]; then
  firmwares=(public advanced)
fi

targets=("$device")
if [[ "$device" == all ]]; then
  targets=("${all_devices[@]}")
fi

cd "$root"
for fw in "${firmwares[@]}"; do
  for dev in "${targets[@]}"; do
    file="$root/firmwares/$fw/devices/$dev.yaml"
    if [[ ! -f "$file" ]]; then
      echo "No such configuration: $file" >&2
      exit 1
    fi
    esphome "$action" "$file"
  done
done
