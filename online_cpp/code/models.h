#pragma once
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>

#ifdef ONLINE_BUILD_PYTHON
#include <nanobind/nanobind.h>
namespace nb = nanobind;
#endif

// --- CONFIGURATION ---
constexpr int    MAX_PACKET_SIZE = 65507;
constexpr double RETRY_INTERVAL  = 1.0;
constexpr int    MAX_RETRIES     = 5;

struct Pending_Msg {
    std::string packet_bytes; // Pre-serialised JSON to resend.
    std::string addr_ip;
    int         addr_port = 0;
    double      send_time = 0.0; // 0 forces immediate first send.
    int         retries   = 0;
};

// Manages reliable-UDP state for one socket: two background threads handle
// receiving/ACKing and retrying un-ACK'd messages.
struct Reliable_UDP_State {
    int fd = -1;

    std::queue<std::string>          incoming;      // Decoded payload JSON strings.
    std::map<std::string, Pending_Msg> pending_acks; // msg_id -> pending message.
    std::set<std::string>            received_ids;  // Deduplication.
    std::mutex                       lock;
    std::atomic<bool>                running{true};

    std::thread receiver_thread;
    std::thread retry_thread;

    explicit Reliable_UDP_State(int socket_fd);
    ~Reliable_UDP_State();

    void receiver_loop();
    void retry_loop();
    void send_ack(const std::string& msg_id, const std::string& ip, int port);
};

// Opaque UDP socket wrapper. Owns the Reliable_UDP_State.
struct UDP_Socket {
    int fd = -1;
    std::shared_ptr<Reliable_UDP_State> state;
};

// Tracks the async state of a P2P connection setup, polled by the UI each frame.
struct Connection_State {
    std::string               room_code;
    std::atomic<bool>         ready{false};
    int                       player_index = 0;
    int                       seed         = 0;
    std::shared_ptr<UDP_Socket> sock;        // Set once the UDP socket is created.
    std::string               friend_ip;
    int                       friend_port  = 0;
    std::string               error;
    std::mutex                state_lock;   // Guards error / friend_ip / friend_port / player_index / seed.

    // Background setup thread; detached, writes to fields above.
    std::thread bg_thread;

    Connection_State()                                 = default;
    Connection_State(const Connection_State&)          = delete;
    Connection_State& operator=(const Connection_State&) = delete;
};
