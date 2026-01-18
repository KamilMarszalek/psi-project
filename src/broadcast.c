#include "broadcast.h"
#include "consts.h"
#include "join.h"
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define JOIN_MAX_MSG_SIZE (sizeof(join_accept_t))

static int
broadcast_send_ack(int broadcast_socket, uint16_t broadcast_port, uint32_t request_id_host, const char* from_name) {
    join_ack_t ack = {0};
    ack.header.magic = htonl(JOIN_MAGIC);
    ack.header.type = htons(JOIN_ACK);
    ack.header.request_id = htonl(request_id_host);
    strncpy(ack.from_name, from_name, MAX_NODE_NAME_SIZE - 1);
    ack.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    destination.sin_port = htons(broadcast_port);

    if (sendto(broadcast_socket, &ack, sizeof(ack), 0, (struct sockaddr*) &destination, sizeof(destination)) < 0) {
        perror("sending join ack");
        return -1;
    }
    return 0;
}

static void apply_accept_if_relevant(ring_state_t* ring_state, const join_accept_t* accept, int* did_apply) {
    *did_apply = 0;
    const char* me = ring_state->config.current->node_name;

    if (strncmp(me, accept->new_name, MAX_NODE_NAME_SIZE) == 0) {
        if (strncmp(me, accept->new_name, MAX_NODE_NAME_SIZE) == 0) {
            if (!ring_state->joined && accept->header.request_id != ring_state->join_request_id) {
                return;
            }
        }
        strncpy(ring_state->config.prev->node_name, accept->prev_name, MAX_NODE_NAME_SIZE - 1);
        ring_state->config.prev->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        ring_state->config.prev->unicast_port = accept->prev_unicast_port;

        strncpy(ring_state->config.next->node_name, accept->before_name, MAX_NODE_NAME_SIZE - 1);
        ring_state->config.next->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        ring_state->config.next->unicast_port = accept->before_unicast_port;
        ring_state->joined = 1;
        ring_state->join_request_last_sent = 0;
        ring_state->join_request_retries = 0;
        *did_apply = 1;
        return;
    }

    if (strncmp(me, accept->prev_name, MAX_NODE_NAME_SIZE) == 0) {
        if (strncmp(ring_state->config.next->node_name, accept->before_name, MAX_NODE_NAME_SIZE) != 0) {
            return;
        }
        strncpy(ring_state->config.next->node_name, accept->new_name, MAX_NODE_NAME_SIZE - 1);
        ring_state->config.next->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        ring_state->config.next->unicast_port = accept->new_unicast_port;
        ring_state->joined = 1;
        *did_apply = 1;
        return;
    }
}

static void handle_ack_inflight(ring_state_t* ring_state, const join_ack_t* ack_host) {
    if (!ring_state->join_inflight.active) {
        return;
    }

    if (ack_host->header.request_id != ring_state->join_inflight.request_id) {
        return;
    }

    const char* from_name = ack_host->from_name;

    if (strncmp(from_name, ring_state->join_inflight.expected_prev_name, MAX_NODE_NAME_SIZE) == 0) {
        ring_state->join_inflight.got_ack_prev = 1;
    }

    if (strncmp(from_name, ring_state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE) == 0) {
        ring_state->join_inflight.got_ack_joiner = 1;
    }

    if (ring_state->join_inflight.got_ack_prev && ring_state->join_inflight.got_ack_joiner) {
        strncpy(ring_state->config.prev->node_name, ring_state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE - 1);
        ring_state->config.prev->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        ring_state->config.prev->unicast_port = ring_state->join_inflight.joiner.unicast_port;

        ring_state->join_inflight.active = 0;
        ring_state->join_inflight.got_ack_prev = 0;
        ring_state->join_inflight.got_ack_joiner = 0;
        ring_state->join_inflight.retries = 0;
    }
}

static int broadcast_send_accept(int broadcast_socket, uint16_t broadcast_port, const join_accept_t* accept) {
    join_accept_t acc = {0};
    acc.header.magic = htonl(JOIN_MAGIC);
    acc.header.type = htons(JOIN_ACCEPT);
    acc.header.reserved = 0;
    acc.header.request_id = htonl(accept->header.request_id);

    strncpy(acc.new_name, accept->new_name, MAX_NODE_NAME_SIZE - 1);
    acc.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    strncpy(acc.before_name, accept->before_name, MAX_NODE_NAME_SIZE - 1);
    acc.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    strncpy(acc.prev_name, accept->prev_name, MAX_NODE_NAME_SIZE - 1);
    acc.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    acc.new_unicast_port = htons(accept->new_unicast_port);
    acc.before_unicast_port = htons(accept->before_unicast_port);
    acc.prev_unicast_port = htons(accept->prev_unicast_port);

    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    destination.sin_port = htons(broadcast_port);

    if (sendto(broadcast_socket, &acc, sizeof(acc), 0, (struct sockaddr*) &destination, sizeof(destination)) < 0) {
        perror("sending join accept");
        return -1;
    }
    return 0;
}

int join_inflight_tick(ring_state_t* ring_state, int broadcast_socket) {
    if (!ring_state->join_inflight.active) {
        return 0;
    }

    time_t now = time(NULL);
    if (now - ring_state->join_inflight.last_sent < JOIN_ACCEPT_TIMEOUT_S && ring_state->join_inflight.last_sent != 0) {
        return 0;
    }

    if (ring_state->join_inflight.retries >= JOIN_ACCEPT_RETRIES) {
        fprintf(stderr, "Join accept retries exhausted for request id %u\n", ring_state->join_inflight.request_id);
        ring_state->join_inflight.active = 0;
        return 0;
    }

    join_accept_t accept = {0};
    accept.header.request_id = ring_state->join_inflight.request_id;

    strncpy(accept.new_name, ring_state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE - 1);
    accept.new_unicast_port = ring_state->join_inflight.joiner.unicast_port;

    strncpy(accept.before_name, ring_state->config.current->node_name, MAX_NODE_NAME_SIZE - 1);
    accept.before_unicast_port = ring_state->config.current->unicast_port;

    strncpy(accept.prev_name, ring_state->config.prev->node_name, MAX_NODE_NAME_SIZE - 1);
    accept.prev_unicast_port = ring_state->config.prev->unicast_port;
    accept.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    (void) broadcast_send_accept(broadcast_socket, ring_state->config.current->broadcast_port, &accept);

    ring_state->join_inflight.last_sent = now;
    ring_state->join_inflight.retries++;

    return 0;
}

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

int handle_broadcast(int broadcast_socket, ring_state_t* ring_state) {
    unsigned char buffer[JOIN_MAX_MSG_SIZE] = {0};
    struct sockaddr_in from = {0};
    socklen_t from_len = sizeof(from);

    ssize_t n_recv = recvfrom(broadcast_socket, buffer, sizeof(buffer), 0, (struct sockaddr*) &from, &from_len);
    if (n_recv < 0) {
        perror("receiving broadcast message");
        return -1;
    }

    if ((size_t) n_recv < sizeof(join_message_header_t)) {
        fprintf(stderr, "Received too small broadcast message\n");
        return 0;
    }

    join_message_header_t header;
    memcpy(&header, buffer, sizeof(header));

    uint32_t magic = ntohl(header.magic);
    uint16_t type = ntohs(header.type);
    uint32_t request_id = ntohl(header.request_id);

    if (magic != JOIN_MAGIC) {
        fprintf(stderr, "Received broadcast message with invalid magic: 0x%X\n", magic);
        return 0;
    }

    char ipbuf[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf))) {
        strncpy(ipbuf, "unknown", sizeof(ipbuf) - 1);
        ipbuf[sizeof(ipbuf) - 1] = '\0';
    }

    switch (type) {
        case JOIN_REQUEST: {
            if ((size_t) n_recv != sizeof(join_request_t)) {
                fprintf(stderr, "Received invalid join request size: %zd\n", n_recv);
                return 0;
            }

            join_request_t join_request;
            memcpy(&join_request, buffer, sizeof(join_request));

            join_request.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';

            if (strncmp(join_request.node_name, ring_state->config.current->node_name, MAX_NODE_NAME_SIZE) == 0) {
                printf("Received join request from self, ignoring\n");
                return 0;
            }

            join_request_t request_host;
            memset(&request_host, 0, sizeof(request_host));
            strncpy(request_host.node_name, join_request.node_name, MAX_NODE_NAME_SIZE - 1);
            request_host.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
            request_host.unicast_port = ntohs(join_request.unicast_port);
            request_host.header.request_id = request_id;
            request_host.header.type = JOIN_REQUEST;
            request_host.header.magic = JOIN_MAGIC;

            int rc = add_pending_join(&ring_state->join_state, &request_host, from.sin_addr);
            if (rc == 0) {
                printf(
                    "JOIN_REQUEST id=%u name=%s uni=%u from=%s\n", request_host.header.request_id,
                    request_host.node_name, request_host.unicast_port, ipbuf
                );
            } else {
                fprintf(stderr, "Failed to add pending join from %s\n", join_request.node_name);
            }
            return 0;
        }

        case JOIN_ACCEPT: {
            if ((size_t) n_recv != sizeof(join_accept_t)) {
                fprintf(stderr, "Received invalid join accept size: %zd\n", n_recv);
                return 0;
            }

            join_accept_t join_accept;
            memcpy(&join_accept, buffer, sizeof(join_accept));

            join_accept.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
            join_accept.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
            join_accept.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

            join_accept_t accept_host;
            memset(&accept_host, 0, sizeof(accept_host));
            accept_host.header.request_id = request_id;
            accept_host.header.type = JOIN_ACCEPT;
            accept_host.header.magic = JOIN_MAGIC;

            strncpy(accept_host.new_name, join_accept.new_name, MAX_NODE_NAME_SIZE - 1);
            strncpy(accept_host.before_name, join_accept.before_name, MAX_NODE_NAME_SIZE - 1);
            strncpy(accept_host.prev_name, join_accept.prev_name, MAX_NODE_NAME_SIZE - 1);
            accept_host.new_unicast_port = ntohs(join_accept.new_unicast_port);
            accept_host.before_unicast_port = ntohs(join_accept.before_unicast_port);
            accept_host.prev_unicast_port = ntohs(join_accept.prev_unicast_port);

            int did_apply = 0;
            apply_accept_if_relevant(ring_state, &accept_host, &did_apply);
            if (did_apply) {
                (void) broadcast_send_ack(
                    broadcast_socket, ring_state->config.current->broadcast_port, request_id,
                    ring_state->config.current->node_name
                );
            }
            return 0;
        }

        case JOIN_ACK: {
            if ((size_t) n_recv != sizeof(join_ack_t)) {
                fprintf(stderr, "Received invalid join ack size: %zd\n", n_recv);
                return 0;
            }

            join_ack_t join_ack;
            memcpy(&join_ack, buffer, sizeof(join_ack));

            join_ack.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';

            join_ack_t ack_host;
            memset(&ack_host, 0, sizeof(ack_host));
            ack_host.header.request_id = request_id;
            strncpy(ack_host.from_name, join_ack.from_name, MAX_NODE_NAME_SIZE - 1);
            ack_host.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';
            ack_host.header.type = JOIN_ACK;
            ack_host.header.magic = JOIN_MAGIC;

            handle_ack_inflight(ring_state, &ack_host);
            return 0;
        }

        default:
            fprintf(stderr, "Received broadcast message with unknown type: %u\n", type);
            return 0;
    }
}

int broadcast_send_join_request(
    int broadcast_socket, uint16_t broadcast_port, uint32_t request_id, const char* node_name, uint16_t unicast_port
) {
    join_request_t request = {0};
    request.header.magic = htonl(JOIN_MAGIC);
    request.header.type = htons(JOIN_REQUEST);
    request.header.request_id = htonl(request_id);
    strncpy(request.node_name, node_name, MAX_NODE_NAME_SIZE - 1);
    request.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    request.unicast_port = htons(unicast_port);

    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    destination.sin_port = htons(broadcast_port);

    if (sendto(broadcast_socket, &request, sizeof(request), 0, (struct sockaddr*) &destination, sizeof(destination)) <
        0) {
        perror("sending join request");
        return -1;
    }
    return 0;
}