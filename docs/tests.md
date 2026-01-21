# Testy pierścienia (Docker)

## Źródła

Opis bazuje na:

- logach w `logs/with-message/`,
- skryptach uruchamiających scenariusze: `scripts/run-with-netem.sh` i
  `scripts/run-all-experiments.sh`.

## Scenariusze testowe

Testy były wykonywane jako scenariusze Docker Compose. W logach znajdują się
m.in.:

- scenariusz z 3 węzłami pierścienia i 4 dołączającymi węzłami,
- ten sam scenariusz z włączonym netem,
- scenariusz z 7 węzłami tworzącymi stały pierścień,
- wariant 7-węzłowy z netem.

Scenariusze z netem uruchamiano z opóźnieniami i stratami pakietów (delay=1000ms
jitter=500ms packet loss=50%).

## Wysyłanie wiadomości do pierścienia

Aby wpuścić wiadomość do pierścienia, należy uruchomić komendę:

```bash
docker exec -it "nazwa kontenera nadawcy" ./build/src/writer "treść" obiorca
```

## Miejsca na fragmenty logów

Poniżej znajdują się fragmenty logów z poszczególnych scenariuszy; wpisy mogą
być przeplatane, ponieważ logi pochodzą z wielu równoległych procesów (kilka
kontenerów) i są scalane w trakcie zbierania, ale nadal pokazują przebieg testu:

### Dołączanie węzłów (wariant 3+4)

Niektóre zduplikowane logi np JOIN_REQUEST zostały usunięte aby skrócić zapis
scenariusza

```c
z11_node0  | [INFO] 20-01-2026 19:51:35: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:51:36: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:51:36: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:51:37: JOIN_REQUEST id=157074605 name=node3 uni=5000 from=172.18.0.2
[...]
z11_node2  | [INFO] 20-01-2026 19:51:37: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:51:37: TOKEN: have_token=1 pending=2 inflight=0
z11_node2  | [INFO] 20-01-2026 19:51:37: JOIN_INFLIGHT START: joiner=node3 req=157074605 (prev=node1 curr=node2)
z11_node5  | [INFO] 20-01-2026 19:51:39: Node initalized, waiting for UDP packets
[...]
z11_node6  | [INFO] 20-01-2026 19:51:41: Node initalized, waiting for UDP packets
z11_node0  | [INFO] 20-01-2026 19:51:43: JOIN_COMMIT: req=157074605 new=node3 topo=1
z11_node2  | [INFO] 20-01-2026 19:51:43: JOIN_INFLIGHT COMPLETE: new prev=node3 topo=1
z11_node3  | [INFO] 20-01-2026 19:51:43: JOIN_COMMIT: req=157074605 new=node3 topo=1
z11_node1  | [INFO] 20-01-2026 19:51:43: JOIN_COMMIT: req=157074605 new=node3 topo=1
z11_node2  | [INFO] 20-01-2026 19:51:45: TOKEN: have_token=1 pending=1 inflight=0
z11_node2  | [INFO] 20-01-2026 19:51:45: JOIN_INFLIGHT START: joiner=node4 req=1227623603 (prev=node3 curr=node2)
z11_node1  | [INFO] 20-01-2026 19:51:49: JOIN_REQUEST id=2769632242 name=node5 uni=5000 from=172.18.0.7
[...]
z11_node1  | [INFO] 20-01-2026 19:51:53: JOIN_COMMIT: req=1227623603 new=node4 topo=2
z11_node2  | [INFO] 20-01-2026 19:51:53: JOIN_INFLIGHT COMPLETE: new prev=node4 topo=2
[...]
z11_node2  | [INFO] 20-01-2026 19:51:57: TOKEN: have_token=1 pending=1 inflight=0
z11_node2  | [INFO] 20-01-2026 19:51:57: JOIN_INFLIGHT START: joiner=node5 req=2769632242 (prev=node4 curr=node2)
[...]
z11_node2  | [INFO] 20-01-2026 19:52:03: JOIN_INFLIGHT COMPLETE: new prev=node5 topo=3
z11_node5  | [INFO] 20-01-2026 19:52:03: JOIN_COMMIT: req=2769632242 new=node5 topo=3
[...]
z11_node2  | [INFO] 20-01-2026 19:52:03: JOIN_REQUEST id=710564936 name=node6 uni=5000 from=172.18.0.8
z11_node2  | [INFO] 20-01-2026 19:52:03: TOKEN: have_token=1 pending=1 inflight=0
z11_node2  | [INFO] 20-01-2026 19:52:03: JOIN_INFLIGHT START: joiner=node6 req=710564936 (prev=node5 curr=node2)
z11_node6  | [INFO] 20-01-2026 19:52:08: JOIN_COMMIT: req=710564936 new=node6 topo=4
[...]
z11_node2  | [INFO] 20-01-2026 19:52:08: JOIN_INFLIGHT COMPLETE: new prev=node6 topo=4
[...]
z11_node2  | [INFO] 20-01-2026 19:52:14: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:52:15: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:52:15: TOKEN: have_token=1 pending=0 inflight=0
[...]
z11_node1  | [INFO] 20-01-2026 19:52:16: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:52:16: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:52:17: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:52:17: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:52:18: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:52:18: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:52:19: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:52:19: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:52:20: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:52:20: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:52:21: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:52:21: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:52:22: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:52:22: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:52:23: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:52:23: TOKEN: have_token=1 pending=0 inflight=0
```

### Dołączanie węzłów z netem

```c
z11_node6  | [INFO] Applying netem: delay=1000ms jitter=500ms loss=50%
z11_node0  | [INFO] 20-01-2026 19:54:45: Node initalized, waiting for UDP packets
z11_node0  | [INFO] 20-01-2026 19:54:45: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:54:45: Node initalized, waiting for UDP packets
z11_node2  | [INFO] 20-01-2026 19:54:45: Node initalized, waiting for UDP packets
z11_node4  | [INFO] 20-01-2026 19:54:47: Node initalized, waiting for UDP packets
z11_node3  | [INFO] 20-01-2026 19:54:47: Node initalized, waiting for UDP packets
z11_node2  | [INFO] 20-01-2026 19:54:48: JOIN_REQUEST id=4239188680 name=node4 uni=5000 from=172.18.0.5
z11_node1  | [INFO] 20-01-2026 19:54:48: JOIN_REQUEST id=4239188680 name=node4 uni=5000 from=172.18.0.5
z11_node5  | [INFO] 20-01-2026 19:54:49: Node initalized, waiting for UDP packets
z11_node1  | [INFO] 20-01-2026 19:54:50: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:54:50: TOKEN: have_token=1 pending=1 inflight=0
z11_node1  | [INFO] 20-01-2026 19:54:50: JOIN_INFLIGHT START: joiner=node4 req=4239188680 (prev=node0 curr=node1)
z11_node0  | [INFO] 20-01-2026 19:54:51: JOIN_REQUEST id=4239188680 name=node4 uni=5000 from=172.18.0.5
z11_node6  | [INFO] 20-01-2026 19:54:51: Node initalized, waiting for UDP packets
[...]
z11_node1  | [INFO] 20-01-2026 19:55:03: JOIN_INFLIGHT COMPLETE: new prev=node4 topo=1
[...]
z11_node1  | [INFO] 20-01-2026 19:55:05: JOIN_REQUEST id=882587745 name=node6 uni=5000 from=172.18.0.6
z11_node1  | [INFO] 20-01-2026 19:55:05: JOIN_INFLIGHT START: joiner=node6 req=882587745 (prev=node4 curr=node1)
[...]
z11_node1  | [INFO] 20-01-2026 19:55:09: JOIN_REQUEST id=1332109065 name=node5 uni=5000 from=172.18.0.4
z11_node1  | [INFO] 20-01-2026 19:55:09: TOKEN: have_token=1 pending=1 inflight=1
z11_node1  | [INFO] 20-01-2026 19:55:16: JOIN_INFLIGHT COMPLETE: new prev=node6 topo=2
[...]
z11_node1  | [INFO] 20-01-2026 19:55:20: JOIN_COMMIT: req=4239188680 new=node4 topo=1
z11_node1  | [INFO] 20-01-2026 19:55:20: JOIN_INFLIGHT START: joiner=node5 req=1332109065 (prev=node6 curr=node1)
z11_node1  | [INFO] 20-01-2026 19:55:24: JOIN_REQUEST id=3305714384 name=node3 uni=5000 from=172.18.0.7
z11_node1  | [INFO] 20-01-2026 19:55:26: JOIN_COMMIT: req=882587745 new=node6 topo=2
[...]
z11_node1  | [INFO] 20-01-2026 19:55:29: JOIN_REQUEST id=3305714384 name=node3 uni=5000 from=172.18.0.7
z11_node1  | [INFO] 20-01-2026 19:55:33: JOIN_REQUEST id=3305714384 name=node3 uni=5000 from=172.18.0.7
z11_node1  | [INFO] 20-01-2026 19:55:48: JOIN_INFLIGHT COMPLETE: new prev=node5 topo=3
z11_node1  | [INFO] 20-01-2026 19:55:49: JOIN_INFLIGHT START: joiner=node3 req=3305714384 (prev=node5 curr=node1)
[...]
z11_node1  | [INFO] 20-01-2026 19:55:53: JOIN_COMMIT: req=1332109065 new=node5 topo=3
z11_node1  | [INFO] 20-01-2026 19:56:03: JOIN_INFLIGHT COMPLETE: new prev=node3 topo=4
z11_node1  | [INFO] 20-01-2026 19:56:05: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:56:09: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:56:09: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:56:13: JOIN_COMMIT: req=3305714384 new=node3 topo=4
z11_node0  | [INFO] 20-01-2026 19:56:24: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:56:24: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:56:36: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:56:36: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:56:41: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:56:41: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:56:53: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:56:53: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:57:02: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:57:02: TOKEN: have_token=1 pending=0 inflight=0
```

### Stabilna praca pierścienia (7 węzłów)

```c
z11_node0  | [INFO] 20-01-2026 19:45:15: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:45:16: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:45:16: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:45:17: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:45:17: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:45:18: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:45:18: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:45:19: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:45:19: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:45:20: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:45:20: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:45:21: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:45:21: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:45:22: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:45:22: TOKEN: have_token=1 pending=0 inflight=0
```

### Stabilna praca z netem

```c
z11_node2  | [INFO] Applying netem: delay=1000ms jitter=500ms loss=50%
z11_node0  | [INFO] 20-01-2026 19:48:24: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:48:39: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:48:39: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:48:51: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:48:51: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:48:58: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:48:58: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:49:13: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:49:13: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:49:25: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:49:25: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:49:37: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:49:37: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:49:51: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:49:51: TOKEN: have_token=1 pending=0 inflight=0
```

### Dodanie wiadomości do pierścienia

Wiadomość została wysłana za pomocą komendy

```bash
docker exec -it z11_node1 ./build/src/writer "kapitan 44" node0
```

```c
z11_node0  | [INFO] 20-01-2026 19:20:01: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:20:01: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:20:02: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:20:02: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:20:02: Attaching CLI token: kapitan 44
z11_node1  | 
z11_node2  | [INFO] 20-01-2026 19:20:03: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:20:03: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:20:03: Full token received for another node - forwarding
z11_node2  | 
z11_node3  | [INFO] 20-01-2026 19:20:04: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:20:04: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:20:04: Full token received for another node - forwarding
z11_node3  | 
z11_node4  | [INFO] 20-01-2026 19:20:05: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:20:05: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:20:05: Full token received for another node - forwarding
z11_node4  | 
z11_node5  | [INFO] 20-01-2026 19:20:06: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:20:06: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:20:06: Full token received for another node - forwarding
z11_node5  | 
z11_node6  | [INFO] 20-01-2026 19:20:07: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:20:07: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:20:07: Full token received for another node - forwarding
z11_node6  | 
z11_node0  | [INFO] 20-01-2026 19:20:08: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:20:08: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:20:08: Received token for me: kapitan 44
```
