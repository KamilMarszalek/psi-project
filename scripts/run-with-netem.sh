#!/usr/bin/env bash

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <delay_ms> [jitter_ms] [loss_%]"
    echo "Example: $0 1000 500 50"
    exit 1
fi

export NETEM_ENABLED=true
export NETEM_DELAY="${1}ms"
export NETEM_JITTER="${2:-0}ms"
export NETEM_LOSS="${3:-0}%"

echo "Network configuration:"
echo "  Delay: $NETEM_DELAY"
echo "  Jitter: $NETEM_JITTER"
echo "  Loss: $NETEM_LOSS"
echo ""

cleanup() {
    echo ""
    echo "Stopping containers"
    kill $(jobs -p) 2>/dev/null
    docker-compose -f docker-compose.yml down
    echo "Cleanup complete!"
}

trap cleanup EXIT INT TERM

echo "Starting containers with netem"
docker-compose -f docker-compose.yml up --build

wait
