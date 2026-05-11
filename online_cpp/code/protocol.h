#pragma once
#include "models.h"

// Returns (or lazily creates) the Reliable_UDP_State for the given UDP_Socket.
Reliable_UDP_State& get_state(UDP_Socket& sock);

// Sends data reliably (non-blocking). Retried in the background until ACK'd.
// data must be a Python dict; addr must be a (str, int) tuple.
void send_message(UDP_Socket& sock, nb::object data, nb::object addr);

// Blocks until a message arrives; returns the payload as a Python dict.
nb::object recv_message(UDP_Socket& sock);

// Non-blocking receive; returns None if no message is available.
nb::object try_recv_message(UDP_Socket& sock);

void bind_protocol(nb::module_& m);
