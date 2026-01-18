#define _GNU_SOURCE
#ifndef BROADCAST_TARGETS_H
#define BROADCAST_TARGETS_H


#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

typedef struct BroadcastTargets {
    char ifname[IF_NAMESIZE];
    struct sockaddr_in addr;
} broadcast_targets_t;

int broadcast_collect_targets(uint16_t port, broadcast_targets_t* out, size_t out_cap);
#endif