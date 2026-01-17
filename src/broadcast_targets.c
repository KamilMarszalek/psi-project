#include "broadcast_targets.h"


static int sockaddr_in_is_valid(const struct sockaddr* addr) {
    return addr && addr->sa_family == AF_INET;
}

static struct in_addr compute_broadcast(struct in_addr ip, struct in_addr netmask) {
    struct in_addr broadcast;
    broadcast.s_addr = ip.s_addr | ~netmask.s_addr;
    return broadcast;
}

int broadcast_collect_targets(uint16_t port, broadcast_targets_t* out, size_t out_cap) {
    struct ifaddrs* ifas;
    if (getifaddrs(&ifas) != 0) {
        perror("getting network interfaces");
        return -1;
    }

    size_t count = 0;
    for (struct ifaddrs* ifa = ifas; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        unsigned flags = ifa->ifa_flags;

        if (!(flags & IFF_UP))
            continue;
        if (!(flags & IFF_BROADCAST))
            continue;
        if (count >= out_cap)
            break;

        struct sockaddr_in* ip = (struct sockaddr_in*) ifa->ifa_addr;
        struct in_addr broadcast_addr = (struct in_addr){0};

        if (sockaddr_in_is_valid(ifa->ifa_broadaddr)) {
            struct sockaddr_in* bcast = (struct sockaddr_in*) ifa->ifa_broadaddr;
            broadcast_addr = bcast->sin_addr;
        } else if (sockaddr_in_is_valid(ifa->ifa_netmask)) {
            struct sockaddr_in* netmask = (struct sockaddr_in*) ifa->ifa_netmask;
            broadcast_addr = compute_broadcast(ip->sin_addr, netmask->sin_addr);
        } else {
            continue;
        }

        memset(&out[count], 0, sizeof(out[count]));
        strncpy(out[count].ifname, ifa->ifa_name, IFNAMSIZ - 1);
        out[count].ifname[IFNAMSIZ - 1] = '\0';

        out[count].addr.sin_family = AF_INET;
        out[count].addr.sin_port = htons(port);
        out[count].addr.sin_addr = broadcast_addr;

        count++;
    }

    freeifaddrs(ifas);
    return (int) count;
}
