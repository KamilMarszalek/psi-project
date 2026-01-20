#ifndef JOIN_FSM_H
#define JOIN_FSM_H

#include "ring.h"

int join_fsm_request_tick(ring_state_t* state, int broadcast_socket);
void join_fsm_start_inflight(ring_state_t* state, const pending_join_t* pending);
int join_fsm_inflight_tick(ring_state_t* state, int unicast_socket);
int join_fsm_handle_accept_unicast(ring_state_t* state, const join_accept_t* accept, int unicast_socket);
void join_fsm_handle_confirm_unicast(ring_state_t* state, const join_confirm_t* confirm);

#endif
