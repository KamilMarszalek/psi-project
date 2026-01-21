#!/usr/bin/env bash

docker exec -it z11_node2 ./build/src/writer "2 do 0" node0
docker exec -it z11_node1 ./build/src/writer "1 do 5" node5
docker exec -it z11_node1 ./build/src/writer "1 do 0" node0
docker exec -it z11_node0 ./build/src/writer "0 do 6" node6
docker exec -it z11_node4 ./build/src/writer "4 do 5" node5
docker exec -it z11_node1 ./build/src/writer "1 do 6" node6
