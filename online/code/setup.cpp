#include "setup.h"

#include <arpa/inet.h>
#include <curl/curl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

#include "protocol.h"

// ---- Utilities ----

std::string generate_room_code(int length) {
  static const char   chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  static std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(
    0, static_cast<int>(sizeof(chars)) - 2
  );
  std::string code;
  code.reserve(static_cast<size_t>(length));
  for (int i = 0; i < length; ++i) code += chars[dist(rng)];
  return code;
}

std::string get_local_ip() {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return "127.0.0.1";
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(80);
  inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return "127.0.0.1";
  }
  sockaddr_in local{};
  socklen_t   len = sizeof(local);
  ::getsockname(fd, reinterpret_cast<sockaddr*>(&local), &len);
  ::close(fd);
  char buf[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
  return std::string(buf);
}

// ---- STUN ----

static const char* STUN_SERVER = "stun.l.google.com";
static const int   STUN_PORT   = 19302;

std::pair<std::string, int> get_ip_info(int fd) {
  addrinfo hints{}, *res = nullptr;
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  if (::getaddrinfo(
        STUN_SERVER, std::to_string(STUN_PORT).c_str(), &hints, &res
      ) != 0)
    throw std::runtime_error("Could not resolve STUN server");

  sockaddr_in stun_addr{};
  std::memcpy(&stun_addr, res->ai_addr, sizeof(stun_addr));
  ::freeaddrinfo(res);

  uint8_t req[20] = {};
  req[0]          = 0x00;
  req[1]          = 0x01;
  req[2]          = 0x00;
  req[3]          = 0x00;
  req[4]          = 0x21;
  req[5]          = 0xA4;
  req[6]          = 0x12;
  req[7]          = 0xAA;

  timeval tv{2, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  std::cout << "[*] Querying STUN server (" << STUN_SERVER << ")...\n";
  ::sendto(
    fd,
    req,
    sizeof(req),
    0,
    reinterpret_cast<sockaddr*>(&stun_addr),
    sizeof(stun_addr)
  );

  uint8_t buf[2048];
  ssize_t n = ::recvfrom(fd, buf, sizeof(buf), 0, nullptr, nullptr);

  timeval zero{0, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &zero, sizeof(zero));

  if (n < 20) throw std::runtime_error("STUN response too short");

  int offset = 20;
  while (offset + 4 <= static_cast<int>(n)) {
    uint16_t attr_type = (static_cast<uint16_t>(buf[offset]) << 8) |
                         buf[offset + 1];
    uint16_t attr_len = (static_cast<uint16_t>(buf[offset + 2]) << 8) |
                        buf[offset + 3];

    if (attr_type == 0x0001 && offset + 12 <= static_cast<int>(n)) {
      int  port = (static_cast<int>(buf[offset + 6]) << 8) | buf[offset + 7];
      char ip_buf[INET_ADDRSTRLEN];
      snprintf(
        ip_buf,
        sizeof(ip_buf),
        "%d.%d.%d.%d",
        buf[offset + 8],
        buf[offset + 9],
        buf[offset + 10],
        buf[offset + 11]
      );
      return {std::string(ip_buf), port};
    }
    offset += 4 + attr_len;
  }
  throw std::runtime_error("STUN response had no MAPPED-ADDRESS attribute");
}

// ---- libcurl helpers ----

static size_t curl_write_cb(
  char* ptr, size_t size, size_t nmemb, std::string* out
) {
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

static void curl_post(const std::string& url, const std::string& body) {
  CURL* c = curl_easy_init();
  if (!c) return;
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
  curl_easy_perform(c);
  curl_easy_cleanup(c);
}

static std::string curl_get(const std::string& url) {
  std::string result;
  CURL*       c = curl_easy_init();
  if (!c) return result;
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &result);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
  curl_easy_perform(c);
  curl_easy_cleanup(c);
  return result;
}

// ---- ntfy.sh helpers ----

static const char* NTFY_URL = "https://ntfy.sh";

void publish_address(
  const std::string& room_code,
  const std::string& ip,
  int                port,
  const std::string& local_ip,
  int                local_port,
  const std::string& suffix
) {
  std::string topic = "gods-" + room_code + suffix;
  std::string url   = std::string(NTFY_URL) + "/" + topic;
  std::string body = "{\"ip\":\"" + ip + "\",\"port\":" + std::to_string(port) +
                     ",\"local_ip\":\"" + local_ip +
                     "\",\"local_port\":" + std::to_string(local_port) + "}";
  curl_post(url, body);
}

static std::string json_str(const std::string& s, const std::string& key) {
  std::string needle = "\"" + key + "\":\"";
  auto        pos    = s.find(needle);
  if (pos == std::string::npos) return "";
  pos += needle.size();
  auto end = s.find('"', pos);
  return (end == std::string::npos) ? "" : s.substr(pos, end - pos);
}

static int json_int(const std::string& s, const std::string& key) {
  std::string needle = "\"" + key + "\":";
  auto        pos    = s.find(needle);
  if (pos == std::string::npos) return 0;
  pos += needle.size();
  while (pos < s.size() && s[pos] == ' ') ++pos;
  return std::stoi(s.substr(pos));
}

std::tuple<std::string, int, std::string, int> fetch_address(
  const std::string& room_code, const std::string& suffix, int timeout_s
) {
  std::string topic = "gods-" + room_code + suffix;
  std::string url   = std::string(NTFY_URL) + "/" + topic +
                    "/json?poll=1&since=all";

  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::seconds(timeout_s);
  while (std::chrono::steady_clock::now() < deadline) {
    std::string body = curl_get(url);
    if (!body.empty()) {
      std::istringstream ss(body);
      std::string        line;
      while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.find("\"event\":\"message\"") == std::string::npos) continue;
        std::string msg_field = json_str(line, "message");
        if (msg_field.empty()) continue;
        std::string pub_ip         = json_str(msg_field, "ip");
        int         pub_port       = json_int(msg_field, "port");
        std::string pub_local_ip   = json_str(msg_field, "local_ip");
        int         pub_local_port = json_int(msg_field, "local_port");
        if (!pub_ip.empty() && pub_port != 0) {
          return {pub_ip, pub_port, pub_local_ip, pub_local_port};
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return {"", 0, "", 0};
}

// ---- Seed exchange (hole-punching + player order) ----

std::pair<int, int> exchange_seeds(
  int fd, bool skip_holepunch, const std::string& friend_ip, int friend_port
) {
  std::mt19937 rng(std::random_device{}());
  int          my_seed = static_cast<int>(rng());

  sockaddr_in friend_addr{};
  friend_addr.sin_family = AF_INET;
  friend_addr.sin_port   = htons(static_cast<uint16_t>(friend_port));
  inet_pton(AF_INET, friend_ip.c_str(), &friend_addr.sin_addr);

  if (!skip_holepunch) {
    for (int i = 0; i < 5; ++i) {
      std::cout << "[*] Sending hole punch packet to " << friend_ip << ":"
                << friend_port << "\n";
      const char punch[] = "PUNCH";
      ::sendto(
        fd,
        punch,
        5,
        0,
        reinterpret_cast<sockaddr*>(&friend_addr),
        sizeof(friend_addr)
      );
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }

  std::string init_msg =
    "{\"type\":\"init\",\"seed\":" + std::to_string(my_seed) + "}";
  ::sendto(
    fd,
    init_msg.c_str(),
    init_msg.size(),
    0,
    reinterpret_cast<sockaddr*>(&friend_addr),
    sizeof(friend_addr)
  );
  std::cout << "[*] Exchanging seeds...\n";

  char    buf[1024];
  timeval tv{30, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  int friend_seed = -1;
  while (true) {
    ssize_t n = ::recvfrom(fd, buf, sizeof(buf) - 1, 0, nullptr, nullptr);
    if (n <= 0) break;
    buf[n] = '\0';
    std::string msg(buf, static_cast<size_t>(n));
    if (msg == "PUNCH") {
      std::cout << "[*] Received hole punch packet from friend.\n";
      continue;
    }
    if (msg.find("\"init\"") != std::string::npos ||
        msg.find("init") != std::string::npos) {
      friend_seed = json_int(msg, "seed");
      break;
    }
  }
  timeval zero{0, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &zero, sizeof(zero));

  if (friend_seed < 0)
    throw std::runtime_error("Failed to receive seed from peer");

  int player_index, game_seed;
  if (my_seed < friend_seed) {
    player_index = 0;
    game_seed    = my_seed;
  } else {
    player_index = 1;
    game_seed    = friend_seed;
  }
  std::cout << "You are Player " << (player_index + 1)
            << ". Seed: " << game_seed << "\n";
  return {player_index, game_seed};
}

// ---- peer_to_peer_cpp (pure C++) ----

P2P_Setup peer_to_peer_cpp(bool local) {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) throw std::runtime_error("Failed to create UDP socket");

  P2P_Setup out;

  if (local) {
    std::cout << "You're playing in local mode.\n";
    out.local_ip = get_local_ip();
    std::cout << "Enter the port to use for the game: ";
    int port = 0;
    std::cin >> port;
    sockaddr_in bind_addr{};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(static_cast<uint16_t>(port));
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    ::bind(fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));
    out.public_ip   = out.local_ip;
    out.public_port = port;
    out.local_port  = port;
  } else {
    sockaddr_in bind_addr{};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = 0;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    ::bind(fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));

    sockaddr_in local_addr{};
    socklen_t   len = sizeof(local_addr);
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&local_addr), &len);
    out.local_port = ntohs(local_addr.sin_port);
    out.local_ip   = get_local_ip();

    auto [pip, pport] = get_ip_info(fd);
    if (pip.empty()) {
      ::close(fd);
      throw std::runtime_error(
        "Could not discover your public IP and port using STUN."
      );
    }
    out.public_ip   = pip;
    out.public_port = pport;
    std::cout << "[*] Public address: " << out.public_ip << ":"
              << out.public_port << "\n";
    std::cout << "[*] Local address: " << out.local_ip << ":" << out.local_port
              << "\n";
  }

  out.sock     = std::make_shared<UDP_Socket>();
  out.sock->fd = fd;
  return out;
}

// ---- setup_online_game (pure C++) ----

Online_Game_Setup setup_online_game(
  UDP_Socket&        sock,
  bool               local,
  const std::string& your_ip,
  int                your_port,
  const std::string& local_ip_arg,
  int                local_port_arg,
  const std::string& room_code_arg
) {
  std::atomic<bool> stop_keepalive{false};
  if (!local) {
    std::thread([&stop_keepalive, fd = sock.fd]() {
      sockaddr_in stun{};
      stun.sin_family = AF_INET;
      stun.sin_port   = htons(STUN_PORT);
      addrinfo hints{}, *res = nullptr;
      hints.ai_family   = AF_INET;
      hints.ai_socktype = SOCK_DGRAM;
      if (::getaddrinfo(
            STUN_SERVER, std::to_string(STUN_PORT).c_str(), &hints, &res
          ) == 0) {
        std::memcpy(&stun, res->ai_addr, sizeof(stun));
        ::freeaddrinfo(res);
      }
      while (!stop_keepalive) {
        ::sendto(
          fd, "", 0, 0, reinterpret_cast<sockaddr*>(&stun), sizeof(stun)
        );
        std::this_thread::sleep_for(std::chrono::seconds(10));
      }
    }).detach();
  }

  std::string friend_ip;
  int         friend_port  = 0;
  bool        same_network = false;
  std::string room_code    = room_code_arg;

  if (local) {
    std::cout << "What is your friend's IP address: ";
    std::cin >> friend_ip;
    std::cout << "What is your friend's port: ";
    std::cin >> friend_port;
  } else if (room_code.empty()) {
    room_code = generate_room_code();
    publish_address(
      room_code, your_ip, your_port, local_ip_arg, local_port_arg
    );
    std::cout << "[*] Room code: " << room_code << "\n";
    std::cout << "[*] Waiting for friend to join...\n";
    auto [fip, fport, flip, flport] = fetch_address(room_code, "-join");
    if (fip.empty()) {
      stop_keepalive = true;
      throw std::runtime_error("Timed out waiting for friend to join.");
    }
    if (fip == your_ip && !flip.empty()) {
      std::cout << "[*] Same network detected, using LAN addresses.\n";
      friend_ip    = flip;
      friend_port  = flport;
      same_network = true;
    } else {
      friend_ip   = fip;
      friend_port = fport;
    }
    std::cout << "[*] Friend joined!\n";
  } else {
    std::cout << "[*] Joining room " << room_code << "...\n";
    auto [fip, fport, flip, flport] = fetch_address(room_code);
    if (fip.empty()) {
      stop_keepalive = true;
      throw std::runtime_error(
        "Could not find room. Check the code and try again."
      );
    }
    publish_address(
      room_code, your_ip, your_port, local_ip_arg, local_port_arg, "-join"
    );
    if (fip == your_ip && !flip.empty()) {
      std::cout << "[*] Same network detected, using LAN addresses.\n";
      friend_ip    = flip;
      friend_port  = flport;
      same_network = true;
    } else {
      friend_ip   = fip;
      friend_port = fport;
    }
    std::cout << "[*] Connected to host!\n";
  }

  stop_keepalive = true;

  auto [player_index, seed] =
    exchange_seeds(sock.fd, local || same_network, friend_ip, friend_port);

  Online_Game_Setup out;
  out.player_index = player_index;
  out.seed         = seed;
  out.sock         = std::make_shared<UDP_Socket>();
  out.sock->fd     = sock.fd;
  out.sock->state  = sock.state;
  out.friend_addr  = std::make_pair(friend_ip, friend_port);
  return out;
}

// ---- Async wrappers ----

std::shared_ptr<Connection_State> start_hosting(bool local) {
  auto state = std::make_shared<Connection_State>();

  state->bg_thread = std::thread([state, local]() {
    try {
      int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
      if (fd < 0) {
        state->error = "Failed to create UDP socket";
        return;
      }

      std::string public_ip, local_ip_val;
      int         public_port = 0, local_port_val = 0;

      sockaddr_in bind_addr{};
      bind_addr.sin_family      = AF_INET;
      bind_addr.sin_port        = 0;
      bind_addr.sin_addr.s_addr = INADDR_ANY;
      ::bind(fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));

      sockaddr_in got{};
      socklen_t   len = sizeof(got);
      ::getsockname(fd, reinterpret_cast<sockaddr*>(&got), &len);
      local_port_val = ntohs(got.sin_port);
      local_ip_val   = get_local_ip();

      auto [pip, pport] = get_ip_info(fd);
      if (pip.empty()) {
        ::close(fd);
        state->error = "Could not discover public IP via STUN.";
        return;
      }
      public_ip   = pip;
      public_port = pport;

      auto sock = std::make_shared<UDP_Socket>();
      sock->fd  = fd;
      {
        std::lock_guard<std::mutex> lg(state->state_lock);
        state->sock = sock;
      }

      std::string room_code = generate_room_code();
      publish_address(
        room_code, public_ip, public_port, local_ip_val, local_port_val
      );
      {
        std::lock_guard<std::mutex> lg(state->state_lock);
        state->room_code = room_code;
      }

      auto [fip, fport, flip, flport] = fetch_address(room_code, "-join");
      if (fip.empty()) {
        state->error = "Timed out waiting for a joiner.";
        return;
      }

      bool same_network = false;
      if (fip == public_ip && !flip.empty()) {
        std::cout << "[*] Same network detected, using LAN addresses.\n";
        fip          = flip;
        fport        = flport;
        same_network = true;
      }

      auto [player_index, seed] =
        exchange_seeds(fd, local || same_network, fip, fport);

      std::lock_guard<std::mutex> lg(state->state_lock);
      state->player_index = player_index;
      state->seed         = seed;
      state->friend_ip    = fip;
      state->friend_port  = fport;
      state->ready        = true;
    } catch (const std::exception& e) {
      state->error = e.what();
    }
  });
  state->bg_thread.detach();
  return state;
}

std::shared_ptr<Connection_State> join_room(
  const std::string& room_code, bool local
) {
  auto state = std::make_shared<Connection_State>();
  {
    std::lock_guard<std::mutex> lg(state->state_lock);
    state->room_code = room_code;
    auto& rc         = state->room_code;
    rc.erase(0, rc.find_first_not_of(" \t\r\n"));
    rc.erase(rc.find_last_not_of(" \t\r\n") + 1);
  }

  state->bg_thread = std::thread([state, local]() {
    try {
      std::string rc;
      {
        std::lock_guard<std::mutex> lg(state->state_lock);
        rc = state->room_code;
      }

      int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
      if (fd < 0) {
        state->error = "Failed to create UDP socket";
        return;
      }

      sockaddr_in bind_addr{};
      bind_addr.sin_family      = AF_INET;
      bind_addr.sin_port        = 0;
      bind_addr.sin_addr.s_addr = INADDR_ANY;
      ::bind(fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));

      sockaddr_in got{};
      socklen_t   len = sizeof(got);
      ::getsockname(fd, reinterpret_cast<sockaddr*>(&got), &len);
      int         local_port_val = ntohs(got.sin_port);
      std::string local_ip_val   = get_local_ip();

      auto [pip, pport] = get_ip_info(fd);
      if (pip.empty()) {
        ::close(fd);
        state->error = "Could not discover public IP via STUN.";
        return;
      }

      auto sock = std::make_shared<UDP_Socket>();
      sock->fd  = fd;
      {
        std::lock_guard<std::mutex> lg(state->state_lock);
        state->sock = sock;
      }

      auto [fip, fport, flip, flport] = fetch_address(rc);
      if (fip.empty()) {
        state->error = "Could not find room. Check the code and try again.";
        return;
      }
      publish_address(rc, pip, pport, local_ip_val, local_port_val, "-join");

      bool same_network = false;
      if (fip == pip && !flip.empty()) {
        std::cout << "[*] Same network detected, using LAN addresses.\n";
        fip          = flip;
        fport        = flport;
        same_network = true;
      }

      auto [player_index, seed] =
        exchange_seeds(fd, local || same_network, fip, fport);

      std::lock_guard<std::mutex> lg(state->state_lock);
      state->player_index = player_index;
      state->seed         = seed;
      state->friend_ip    = fip;
      state->friend_port  = fport;
      state->ready        = true;
    } catch (const std::exception& e) {
      state->error = e.what();
    }
  });
  state->bg_thread.detach();
  return state;
}

// ---- Nanobind bindings ----
