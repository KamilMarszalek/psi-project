#include "broadcast.h"
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int broadcast_setup_socket(const route_t* current) {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        perror("opening broadcast socket");
        return -1;
    }

    int broadcast_enable = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("setting broadcast option");
        close(sock_fd);
        return -1;
    }

    struct sockaddr_in host_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(current->port),
    };
    socklen_t length = sizeof(host_addr);
    if (bind(sock_fd, (struct sockaddr*) &host_addr, length) < 0) {
        perror("binding broadcast socket");
        close(sock_fd);
        return -1;
    }
    return sock_fd;
}