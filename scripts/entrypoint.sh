#!/usr/bin/env bash

if [ "$NETEM_ENABLED" = "true" ]; then
    echo "[INFO] Applying netem: delay=${NETEM_DELAY:-0ms} jitter=${NETEM_JITTER:-0ms} loss=${NETEM_LOSS:-0%}"
    tc qdisc add dev eth0 root netem delay ${NETEM_DELAY:-0ms} ${NETEM_JITTER:-0ms} loss ${NETEM_LOSS:-0%}
fi

exec "$@"
