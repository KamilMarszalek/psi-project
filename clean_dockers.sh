docker compose down

docker ps --format '{{.Names}}' | grep '^z11_node' | xargs -r docker stop
docker ps --format '{{.Names}}' | grep '^z11_node' | xargs -r docker stop