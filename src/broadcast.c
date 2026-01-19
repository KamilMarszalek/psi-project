#include "broadcast.h"
#include "broadcast_targets.h"
#include "consts.h"
#include "join.h"
#include "logger.h"
#include <errno.h>


#define JOIN_MAX_MSG_SIZE (sizeof(join_accept_t))

static broadcast_targets_t targets[16];
static size_t targets_count = 0;

static int send_to_all_targets(int sock, const void* msg, size_t len, const broadcast_targets_t* targets, size_t n) {
    int ok = 0;
    for (size_t i = 0; i < n; i++) {
        if (sendto(sock, msg, len, 0, (const struct sockaddr*) &targets[i].addr, sizeof(targets[i].addr)) < 0) {
            LOG_WARN("sendto broadcast failed on %s: %s", targets[i].ifname, strerror(errno));
        } else {
            ok = 1;
        }
    }
    if (n == 0)
        return -1;
    return ok ? 0 : -1;
}

static int should_ack_accept(const ring_state_t* st, const join_accept_t* acc) {
    const char* me = st->config.current->node_name;

    if (strncmp(me, acc->new_name, MAX_NODE_NAME_SIZE) == 0) {
        if (!st->joined && acc->header.request_id != st->join_request_id) {
            return 0;
        }
        return 1;
    }

    if (strncmp(me, acc->prev_name, MAX_NODE_NAME_SIZE) == 0) {
        if (strncmp(st->config.next->node_name, acc->before_name, MAX_NODE_NAME_SIZE) == 0)
            return 1;
        if (strncmp(st->config.next->node_name, acc->new_name, MAX_NODE_NAME_SIZE) == 0)
            return 1;
    }

    return 0;
}


static int broadcast_send_ack(int broadcast_socket, uint32_t request_id_host, const char* from_name) {
    join_ack_t ack = {0};
    ack.header.magic = htonl(JOIN_MAGIC);
    ack.header.type = htons(JOIN_ACK);
    ack.header.request_id = htonl(request_id_host);
    strncpy(ack.from_name, from_name, MAX_NODE_NAME_SIZE - 1);
    ack.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    LOG_INFO("SENDING JOIN_ACK: req=%u from=%s", request_id_host, from_name);
    if (send_to_all_targets(broadcast_socket, &ack, sizeof(ack), targets, targets_count) < 0) {
        LOG_ERROR("sending join ack");
        return -1;
    }
    return 0;
}

static void apply_accept_if_relevant(ring_state_t* ring_state, const join_accept_t* accept, int* did_apply) {
    *did_apply = 0;
    const char* me = ring_state->config.current->node_name;

    if (strncmp(me, accept->new_name, MAX_NODE_NAME_SIZE) == 0) {
        if (!ring_state->joined && accept->header.request_id != ring_state->join_request_id) {
            return;
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

static int broadcast_send_accept(int broadcast_socket, const join_accept_t* accept) {
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

    if (send_to_all_targets(broadcast_socket, &acc, sizeof(acc), targets, targets_count) < 0) {
        LOG_ERROR("sending join accept");
        return -1;
    }
    return 0;
}

int join_inflight_tick(ring_state_t* ring_state, int broadcast_socket) {
    LOG_INFO(
        "JOIN_ACCEPT TICK: active=%d req=%u retries=%d targets=%zu", ring_state->join_inflight.active,
        ring_state->join_inflight.request_id, ring_state->join_inflight.retries, targets_count
    );

    if (!ring_state->join_inflight.active) {
        return 0;
    }

    time_t now = time(NULL);
    if (now - ring_state->join_inflight.last_sent < JOIN_ACCEPT_TIMEOUT_S && ring_state->join_inflight.last_sent != 0) {
        return 0;
    }

    if (ring_state->join_inflight.retries >= JOIN_ACCEPT_RETRIES) {
        LOG_ERROR("Join accept retries exhausted for request id %u", ring_state->join_inflight.request_id);
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

    LOG_INFO("SENDING JOIN_ACCEPT: new=%s before=%s prev=%s", accept.new_name, accept.before_name, accept.prev_name);

    if (broadcast_send_accept(broadcast_socket, &accept) < 0) {
        return -1;
    }

    ring_state->join_inflight.last_sent = now;
    ring_state->join_inflight.retries++;

    return 0;
}

int broadcast_setup_socket(const route_t* current) {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        LOG_ERROR("opening broadcast socket");
        return -1;
    }

    int broadcast_enable = 1;
    int reuse_enable = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        LOG_ERROR("setting broadcast option");
        close(sock_fd);
        return -1;
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_enable, sizeof(reuse_enable)) < 0) {
        LOG_ERROR("setting reuse address option");
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
        LOG_ERROR("binding broadcast socket");
        close(sock_fd);
        return -1;
    }
    int n = broadcast_collect_targets(current->broadcast_port, targets, sizeof(targets) / sizeof(targets[0]));
    if (n < 0) {
        close(sock_fd);
        return -1;
    }
    targets_count = (size_t) n;
    if (targets_count == 0) {
        LOG_WARN("No broadcast targets found");
    }
    for (size_t i = 0; i < targets_count; i++) {
        char b[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &targets[i].addr.sin_addr, b, sizeof(b));
        LOG_INFO("BCAST TARGET[%zu]: if=%s addr=%s port=%u", i, targets[i].ifname, b, ntohs(targets[i].addr.sin_port));
    }

    return sock_fd;
}

int handle_broadcast(int broadcast_socket, ring_state_t* ring_state) {
    unsigned char buffer[JOIN_MAX_MSG_SIZE] = {0};
    struct sockaddr_in from = {0};
    socklen_t from_len = sizeof(from);

    ssize_t n_recv = recvfrom(broadcast_socket, buffer, sizeof(buffer), 0, (struct sockaddr*) &from, &from_len);
    if (n_recv < 0) {
        LOG_ERROR("receiving broadcast message");
        return -1;
    }

    if ((size_t) n_recv < sizeof(join_message_header_t)) {
        LOG_ERROR("Received too small broadcast message");
        return 0;
    }

    join_message_header_t header;
    memcpy(&header, buffer, sizeof(header));

    uint32_t magic = ntohl(header.magic);
    uint16_t type = ntohs(header.type);
    uint32_t request_id = ntohl(header.request_id);

    if (magic != JOIN_MAGIC) {
        LOG_ERROR("Received broadcast message with invalid magic: 0x%X", magic);
        return 0;
    }

    if (!ring_state->joined && type != JOIN_ACCEPT) {
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
                LOG_ERROR("Received invalid join request size: %zd", n_recv);
                return 0;
            }

            join_request_t join_request;
            memcpy(&join_request, buffer, sizeof(join_request));

            join_request.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';

            if (strncmp(join_request.node_name, ring_state->config.current->node_name, MAX_NODE_NAME_SIZE) == 0) {
                LOG_INFO("Received join request from self, ignoring");
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
                LOG_INFO(
                    "JOIN_REQUEST id=%u name=%s uni=%u from=%s", request_host.header.request_id, request_host.node_name,
                    request_host.unicast_port, ipbuf
                );
            } else {
                LOG_ERROR("Failed to add pending join from %s", join_request.node_name);
            }
            return 0;
        }

        case JOIN_ACCEPT: {


            if ((size_t) n_recv != sizeof(join_accept_t)) {
                LOG_ERROR("Received invalid join accept size: %zd", n_recv);
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

            LOG_INFO(
                "RECV JOIN_ACCEPT: req=%u new=%s before=%s prev=%s", accept_host.header.request_id,
                accept_host.new_name, accept_host.before_name, accept_host.prev_name
            );
            int should_ack = should_ack_accept(ring_state, &accept_host);
            int did_apply = 0;
            apply_accept_if_relevant(ring_state, &accept_host, &did_apply);
            LOG_INFO("APPLY JOIN_ACCEPT: did_apply=%d", did_apply);
            if (should_ack) {
                int re = broadcast_send_ack(broadcast_socket, request_id, ring_state->config.current->node_name);
                if (re < 0) {
                    LOG_ERROR("Failed to send JOIN_ACK");
                    return -1;
                }
            }
            return 0;
        }

        case JOIN_ACK: {
            if ((size_t) n_recv != sizeof(join_ack_t)) {
                LOG_ERROR("Received invalid join ack size: %zd", n_recv);
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
            LOG_INFO(
                "RECV JOIN_ACK: req=%u from=%s inflight=%d exp_prev=%s joiner=%s got_prev=%d got_joiner=%d",
                ack_host.header.request_id, ack_host.from_name, ring_state->join_inflight.active,
                ring_state->join_inflight.expected_prev_name, ring_state->join_inflight.joiner.node_name,
                ring_state->join_inflight.got_ack_prev, ring_state->join_inflight.got_ack_joiner
            );


            handle_ack_inflight(ring_state, &ack_host);
            LOG_INFO(
                "ACK_STATE AFTER: inflight=%d got_prev=%d got_joiner=%d", ring_state->join_inflight.active,
                ring_state->join_inflight.got_ack_prev, ring_state->join_inflight.got_ack_joiner
            );
            return 0;
        }

        default:
            LOG_ERROR("Received broadcast message with unknown type: %u", type);
            return 0;
    }
}

int broadcast_send_join_request(
    int broadcast_socket, uint32_t request_id, const char* node_name, uint16_t unicast_port
) {
    join_request_t request = {0};
    request.header.magic = htonl(JOIN_MAGIC);
    request.header.type = htons(JOIN_REQUEST);
    request.header.request_id = htonl(request_id);
    strncpy(request.node_name, node_name, MAX_NODE_NAME_SIZE - 1);
    request.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    request.unicast_port = htons(unicast_port);

    if (send_to_all_targets(broadcast_socket, &request, sizeof(request), targets, targets_count) < 0) {
        LOG_ERROR("sending join request");
        return -1;
    }
    return 0;
}