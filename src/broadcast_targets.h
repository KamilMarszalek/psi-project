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
#endif