#pragma once
#include "models.h"

std::string generate_room_code(int length = 4);
std::string get_local_ip();

// Returns (public_ip, public_port). Uses the same socket that will be used for gameplay
// so NAT keeps the same external port open.
std::pair<std::string, int> get_ip_info(int fd);

void publish_address(const std::string& room_code, const std::string& ip, int port,
                     const std::string& local_ip, int local_port,
                     const std::string& suffix = "");

// Returns (public_ip, public_port, local_ip, local_port). Empty strings on timeout.
std::tuple<std::string, int, std::string, int>
fetch_address(const std::string& room_code, const std::string& suffix = "", int timeout_s = 120);

// Returns (player_index, seed).
std::pair<int, int> exchange_seeds(int fd, bool skip_holepunch,
                                   const std::string& friend_ip, int friend_port);

// Full P2P setup: returns (sock, public_ip, public_port, local_ip, local_port).
// The UDP_Socket is created inside; caller owns it.
nb::tuple peer_to_peer(bool local = false);

// Exchange addresses and seeds with the peer. Returns (player_index, seed, sock, friend_addr).
nb::tuple setup_online_game(UDP_Socket& sock, bool local,
                             const std::string& your_ip, int your_port,
                             const std::string& local_ip, int local_port,
                             const std::string& room_code = "");

// Async wrappers used by the graphical menu.
std::shared_ptr<Connection_State> start_hosting(bool local = false);
std::shared_ptr<Connection_State> join_room(const std::string& room_code, bool local = false);

void bind_setup(nb::module_& m);
