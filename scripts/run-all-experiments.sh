#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DURATION_SECONDS=90
NETEM_DELAY=1000
NETEM_JITTER=500
NETEM_LOSS=50

usage() {
  echo "Usage: $0 <compose-file> [compose-file ...]"
  echo "Example: $0 docker-compose.1ring-4join.yml docker-compose.3ring-4join.yml"
}

current_pid=""

stop_current() {
  if [[ -n "$current_pid" ]]; then
    kill "$current_pid" 2>/dev/null || true
    wait "$current_pid" 2>/dev/null || true
    current_pid=""
  fi
}

cleanup() {
  stop_current
}

handle_interrupt() {
  echo ""
  echo "Interrupted; stopping current experiment"
  stop_current
  exit 130
}

trap cleanup EXIT TERM
trap handle_interrupt INT

run_experiment() {
  local compose_file="$1"
  shift

  if [ ! -f "$compose_file" ]; then
    echo "Compose file not found: $compose_file"
    exit 1
  fi

  echo "Running ${compose_file} $* for ${DURATION_SECONDS}s"

  bash "${SCRIPT_DIR}/run-with-netem.sh" "$compose_file" "$@" &
  current_pid=$!

  local end_time=$((SECONDS + DURATION_SECONDS))
  while kill -0 "$current_pid" 2>/dev/null; do
    if [ "$SECONDS" -ge "$end_time" ]; then
      stop_current
      break
    fi
    sleep 1
  done
}

if [ "$#" -lt 1 ]; then
  usage
  exit 1
fi

for compose_file in "$@"; do
  run_experiment "$compose_file"
  run_experiment "$compose_file" --netem "$NETEM_DELAY" "$NETEM_JITTER" "$NETEM_LOSS"
done
