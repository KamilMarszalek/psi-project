#!/usr/bin/env bash
set -euo pipefail

NET="z11_network"

usage() {
	echo "Usage: $0 <compose-file> [--netem <delay_ms> [jitter_ms] [loss_%]]"
	echo "Example: $0 docker-compose.1ring-4join.yml --netem 1000 500 50"
	echo "Example: $0 docker-compose.3ring-4join.yml"
}

if [ "$#" -lt 1 ]; then
	usage
	exit 1
fi

compose_file="$1"
shift

netem_suffix=""
NETEM_ENABLED=false
NETEM_DELAY="0ms"
NETEM_JITTER="0ms"
NETEM_LOSS="0%"

if [[ "${1:-}" == "--netem" ]]; then
	if [ "$#" -lt 2 ]; then
		echo "Missing netem delay_ms value"
		usage
		exit 1
	fi
	NETEM_ENABLED=true
	NETEM_DELAY="${2}"
	NETEM_JITTER="${3:-0}"
	NETEM_LOSS="${4:-0}"
	netem_suffix="-netem"
elif [[ -n "${1:-}" && "${1:-}" != "--no-netem" ]]; then
	echo "Unknown option: $1"
	usage
	exit 1
fi

export NETEM_ENABLED
export NETEM_DELAY
export NETEM_JITTER
export NETEM_LOSS

if command -v docker-compose >/dev/null 2>&1; then
	docker_compose_cmd=(docker-compose)
elif docker compose version >/dev/null 2>&1; then
	docker_compose_cmd=(docker compose)
else
	echo "Docker Compose not found (docker-compose or docker compose)"
	exit 1
fi

scenario_name="$(basename "$compose_file")"
scenario_name="${scenario_name%.yml}"
scenario_name="${scenario_name%.yaml}"
log_dir="logs/${scenario_name}${netem_suffix}"

cleanup() {
	echo ""
	echo "Stopping containers"
	if [ "${#log_pids[@]:-0}" -gt 0 ]; then
		kill "${log_pids[@]}" 2>/dev/null || true
	fi
	"${docker_compose_cmd[@]}" -f "$compose_file" down
	echo "Cleanup complete"
}

trap cleanup EXIT INT TERM

echo "Compose file: $compose_file"
echo "Netem enabled: $NETEM_ENABLED"
if [[ "$NETEM_ENABLED" == "true" ]]; then
	echo "  Delay: $NETEM_DELAY"
	echo "  Jitter: $NETEM_JITTER"
	echo "  Loss: $NETEM_LOSS"
fi

docker network create "$NET" 2>/dev/null || true

echo "Starting containers"
"${docker_compose_cmd[@]}" -f "$compose_file" up -d --build

mkdir -p "$log_dir"
log_pids=()

mapfile -t services < <("${docker_compose_cmd[@]}" -f "$compose_file" config --services)

if [ "${#services[@]}" -eq 0 ]; then
	echo "No services found for compose file"
	exit 1
fi

echo "Streaming logs to $log_dir"
"${docker_compose_cmd[@]}" -f "$compose_file" logs -f --no-color >"$log_dir/combined" 2>&1 &
log_pids+=("$!")

for service in "${services[@]}"; do
	"${docker_compose_cmd[@]}" -f "$compose_file" logs -f --no-color "$service" >"$log_dir/${service}.log" 2>&1 &
	log_pids+=("$!")
done

wait
