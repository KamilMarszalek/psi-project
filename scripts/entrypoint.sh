#!/usr/bin/env bash

if [ "$NETEM_ENABLED" = "true" ]; then
  echo "[INFO] Applying netem: delay=${NETEM_DELAY:-0}ms jitter=${NETEM_JITTER:-0}ms loss=${NETEM_LOSS:-0}%"
  tc qdisc add dev eth0 root netem delay "${NETEM_DELAY:-0}"ms "${NETEM_JITTER:-0}"ms loss "${NETEM_LOSS:-0}"%
fi

if [ -n "${START_DELAY_SECONDS:-}" ]; then
  echo "[INFO] Delaying start by ${START_DELAY_SECONDS}s"
  sleep "${START_DELAY_SECONDS}"
fi

exec "$@"
