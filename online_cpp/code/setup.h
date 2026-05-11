#pragma once
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "models.h"

std::string generate_room_code(int length = 4);
std::string get_local_ip();

// Returns (public_ip, public_port).
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

// Pure C++ P2P setup result.
struct P2P_Setup {
    std::shared_ptr<UDP_Socket> sock;
    std::string                 public_ip;
    int                         public_port = 0;
    std::string                 local_ip;
    int                         local_port  = 0;
};

P2P_Setup peer_to_peer_cpp(bool local = false);

// Pure C++ online game setup result.
struct Online_Game_Setup {
    int                              player_index = 0;
    int                              seed         = 0;
    std::shared_ptr<UDP_Socket>      sock;
    std::pair<std::string, int>      friend_addr;
};

Online_Game_Setup setup_online_game_cpp(UDP_Socket& sock, bool local,
                                         const std::string& your_ip, int your_port,
                                         const std::string& local_ip, int local_port,
                                         const std::string& room_code = "");

// Async wrappers used by the graphical menu.
std::shared_ptr<Connection_State> start_hosting(bool local = false);
std::shared_ptr<Connection_State> join_room(const std::string& room_code, bool local = false);
