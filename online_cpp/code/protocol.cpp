#include "protocol.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

// --- UUID generation (8 hex chars, enough for msg IDs) ---

static std::string make_msg_id() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist;
    uint32_t v = dist(rng);
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

static double now_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// --- Reliable_UDP_State implementation ---

Reliable_UDP_State::Reliable_UDP_State(int socket_fd) : fd(socket_fd) {
    receiver_thread = std::thread(&Reliable_UDP_State::receiver_loop, this);
    retry_thread    = std::thread(&Reliable_UDP_State::retry_loop, this);
}

Reliable_UDP_State::~Reliable_UDP_State() {
    running = false;
    // Closing the fd unblocks recvfrom in the receiver thread.
    // The fd itself is owned by UDP_Socket; we just signal threads to stop.
    if (receiver_thread.joinable()) receiver_thread.join();
    if (retry_thread.joinable())    retry_thread.join();
}

void Reliable_UDP_State::send_ack(const std::string& msg_id, const std::string& ip, int port) {
    std::string ack = "{\"t\":\"ACK\",\"i\":\"" + msg_id + "\"}";
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    ::sendto(fd, ack.c_str(), ack.size(), 0,
             reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

void Reliable_UDP_State::receiver_loop() {
    char buf[MAX_PACKET_SIZE];
    while (running) {
        sockaddr_in sender{};
        socklen_t   sender_len = sizeof(sender);
        ssize_t     n = ::recvfrom(fd, buf, sizeof(buf), 0,
                                    reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n <= 0) break; // Socket closed or error.

        std::string raw(buf, static_cast<size_t>(n));

        // Parse minimal fields without a JSON library.
        // Packet format: {"t":"DATA","i":"<id>","p":<payload>}
        // or             {"t":"ACK","i":"<id>"}
        auto extract = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            auto pos = raw.find(needle);
            if (pos == std::string::npos) return "";
            pos += needle.size();
            auto end = raw.find('"', pos);
            return (end == std::string::npos) ? "" : raw.substr(pos, end - pos);
        };

        std::string msg_type = extract("t");
        std::string msg_id   = extract("i");
        if (msg_type.empty() || msg_id.empty()) continue;

        char sender_ip_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender.sin_addr, sender_ip_buf, sizeof(sender_ip_buf));
        std::string sender_ip(sender_ip_buf);
        int         sender_port = ntohs(sender.sin_port);

        if (msg_type == "ACK") {
            std::lock_guard<std::mutex> lg(lock);
            pending_acks.erase(msg_id);
        } else if (msg_type == "DATA") {
            // Always ACK.
            send_ack(msg_id, sender_ip, sender_port);

            std::lock_guard<std::mutex> lg(lock);
            if (received_ids.count(msg_id) == 0) {
                received_ids.insert(msg_id);
                // Extract the "p" value (everything after "p": to the matching
                // closing brace of the outer object).
                std::string p_needle = "\"p\":";
                auto p_pos = raw.find(p_needle);
                if (p_pos != std::string::npos) {
                    std::string payload = raw.substr(p_pos + p_needle.size());
                    if (!payload.empty() && payload.back() == '}') {
                        int depth = 0;
                        bool in_str = false;
                        for (size_t i = 0; i < payload.size(); ++i) {
                            char c = payload[i];
                            if (in_str) { if (c == '\\') ++i; else if (c == '"') in_str = false; }
                            else if (c == '"') in_str = true;
                            else if (c == '{' || c == '[') ++depth;
                            else if (c == '}' || c == ']') {
                                --depth;
                                if (depth < 0) {
                                    // This closing brace belongs to the envelope.
                                    payload = payload.substr(0, i);
                                    break;
                                }
                            }
                        }
                    }
                    incoming.push(payload);
                }
            }
        }
    }
}

void Reliable_UDP_State::retry_loop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        double now = now_seconds();
        std::lock_guard<std::mutex> lg(lock);
        for (auto it = pending_acks.begin(); it != pending_acks.end(); ) {
            Pending_Msg& msg = it->second;
            if (now - msg.send_time > RETRY_INTERVAL) {
                if (msg.retries < MAX_RETRIES) {
                    sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    addr.sin_port   = htons(static_cast<uint16_t>(msg.addr_port));
                    inet_pton(AF_INET, msg.addr_ip.c_str(), &addr.sin_addr);
                    ::sendto(fd, msg.packet_bytes.c_str(), msg.packet_bytes.size(), 0,
                             reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
                    msg.send_time = now;
                    ++msg.retries;
                    ++it;
                } else {
                    it = pending_acks.erase(it);
                }
            } else {
                ++it;
            }
        }
    }
}

// --- Pure C++ API ---

Reliable_UDP_State& get_state(UDP_Socket& sock) {
    if (!sock.state) sock.state = std::make_shared<Reliable_UDP_State>(sock.fd);
    return *sock.state;
}

void send_message(UDP_Socket& sock, const nlohmann::json& data, const std::pair<std::string, int>& addr) {
    std::string payload = data.dump();
    std::string msg_id  = make_msg_id();

    std::string packet = "{\"t\":\"DATA\",\"i\":\"" + msg_id + "\",\"p\":" + payload + "}";

    Pending_Msg pm;
    pm.packet_bytes = std::move(packet);
    pm.addr_ip      = addr.first;
    pm.addr_port    = addr.second;
    pm.send_time    = 0.0; // 0 forces immediate first send by retry_loop.
    pm.retries      = 0;

    Reliable_UDP_State& st = get_state(sock);
    std::lock_guard<std::mutex> lg(st.lock);
    st.pending_acks[msg_id] = std::move(pm);
}

nlohmann::json recv_message(UDP_Socket& sock) {
    Reliable_UDP_State& st = get_state(sock);
    std::string payload;
    while (true) {
        {
            std::lock_guard<std::mutex> lg(st.lock);
            if (!st.incoming.empty()) {
                payload = std::move(st.incoming.front());
                st.incoming.pop();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return nlohmann::json::parse(payload);
}

std::optional<nlohmann::json> try_recv_message(UDP_Socket& sock) {
    Reliable_UDP_State& st = get_state(sock);
    std::string payload;
    {
        std::lock_guard<std::mutex> lg(st.lock);
        if (st.incoming.empty()) return std::nullopt;
        payload = std::move(st.incoming.front());
        st.incoming.pop();
    }
    return nlohmann::json::parse(payload);
}

// --- Python bindings (adapter layer) ---
