#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DURATION_SECONDS=90
NETEM_DELAY=1000
NETEM_JITTER=500
NETEM_LOSS=50

compose_files=(
	"docker-compose.1ring-4join.yml"
	"docker-compose.3ring-4join.yml"
)

run_experiment() {
	local compose_file="$1"
	shift

	echo "Running ${compose_file} $* for ${DURATION_SECONDS}s"
	timeout "${DURATION_SECONDS}s" "${SCRIPT_DIR}/run-with-netem.sh" "$compose_file" "$@"
}

for compose_file in "${compose_files[@]}"; do
	run_experiment "$compose_file"
	run_experiment "$compose_file" --netem "$NETEM_DELAY" "$NETEM_JITTER" "$NETEM_LOSS"
done
