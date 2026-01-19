#!/usr/bin/env bash
set -euo pipefail

NET="z11_network"
IMG="z11_token_ring"
SLEEP_SECS=10

COUNT="${1:-}"

if [[ -z "$COUNT" || ! "$COUNT" =~ ^[0-9]+$ ]]; then
  echo "Usage: $0 <count>"
  echo "Example: $0 10"
  exit 1
fi

docker network create "$NET" 2>/dev/null || true
docker compose up -d --build

start_i=3
end_i=$((start_i + COUNT - 1))

sleep 5

for ((i=start_i; i<=end_i; i++)); do
  name="node${i}"

  docker run -d --rm \
    --name "z11_${name}" \
    --network "$NET" \
    --network-alias "$name" \
    -e NODE_NAME="$name" \
    -e NODE_UNI_PORT="5000" \
    -e NODE_BROAD_PORT="6000" \
    "$IMG" 0

  echo "Started ${name} (${i}/${end_i})"
  sleep "$SLEEP_SECS"
done

echo "Done. Started ${COUNT} containers."
