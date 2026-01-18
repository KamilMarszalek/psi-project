#ifndef BROADCAST_TARGETS_H
#define BROADCAST_TARGETS_H
#define _GNU_SOURCE

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

typedef struct BroadcastTargets {
    char ifname[IFNAMSIZ];
    struct sockaddr_in addr;
} broadcast_targets_t;

int broadcast_collect_targets(uint16_t port, broadcast_targets_t* out, size_t out_cap);
#endif