#include "broadcast.h"
#include "consts.h"
#include "join.h"
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
        .sin_port = htons(current->broadcast_port),
    };
    socklen_t length = sizeof(host_addr);
    if (bind(sock_fd, (struct sockaddr*) &host_addr, length) < 0) {
        perror("binding broadcast socket");
        close(sock_fd);
        return -1;
    }
    return sock_fd;
}

int handle_broadcast(int broadcast_socket) {
    join_request_t join_request;
    struct sockaddr_in from = {0};
    socklen_t from_len = sizeof(from);

    ssize_t n_recv =
        recvfrom(broadcast_socket, &join_request, sizeof(join_request), 0, (struct sockaddr*) &from, &from_len);

    if (n_recv < 0) {
        perror("receiving broadcast message");
        return -1;
    }

    if ((size_t) n_recv != sizeof(join_request)) {
        fprintf(stderr, "Received invalid broadcast message size: %zd\n", n_recv);
        return -1;
    }

    if (join_request.magic != JOIN_MAGIC) {
        fprintf(stderr, "Received invalid broadcast message magic: 0x%X\n", join_request.magic);
        return -1;
    }

    char ipbuf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf)) == NULL) {
        perror("converting sender address to string");
        return -1;
    }
    printf("Received join request from node %s at %s:%d\n", join_request.node_name, ipbuf, ntohs(from.sin_port));
    return 0;
}