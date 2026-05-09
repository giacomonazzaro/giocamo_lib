from __future__ import annotations
import json
import socket
import threading
import time
import uuid
import queue

# --- CONFIGURATION ---
MAX_PACKET_SIZE = 65507  # Max safe UDP payload
RETRY_INTERVAL = 1.0     # Seconds between retries
MAX_RETRIES = 5          # Give up after 5 tries

class ReliableUDPState:
    """
    A helper class to manage the complex state of Reliable UDP
    (background listening, retries, ACKs) without blocking your main app.
    """
    def __init__(self, sock: socket.socket):
        self.sock = sock
        
        self.incoming_queue = queue.Queue()
        self.pending_acks = {}  # {msg_id: {'data': dict, 'addr': tuple, 'time': float, 'retries': int}}
        self.received_ids = set()
        self.running = True

        # Start background workers immediately
        threading.Thread(target=self._receiver_loop, daemon=True).start()
        threading.Thread(target=self._retry_loop, daemon=True).start()

    def _receiver_loop(self):
        """Constantly reads UDP packets to catch ACKs and Data."""
        while self.running:
            try:
                data_bytes, addr = self.sock.recvfrom(MAX_PACKET_SIZE)
                try:
                    packet = json.loads(data_bytes.decode("utf-8"))
                except json.JSONDecodeError:
                    continue

                msg_type = packet.get("t")
                msg_id = packet.get("i")

                # CASE 1: We received an ACK for a message we sent
                if msg_type == "ACK":
                    if msg_id in self.pending_acks:
                        del self.pending_acks[msg_id] # Stop retrying

                # CASE 2: We received a DATA message
                elif msg_type == "DATA":
                    # 1. Always ACK immediately
                    self._send_ack(msg_id, addr)

                    # 2. If it's new, put it in the queue for the user
                    if msg_id not in self.received_ids:
                        self.received_ids.add(msg_id)
                        # We only want the user's payload, not our protocol headers
                        self.incoming_queue.put((packet["p"], addr))
            
            except OSError:
                break # Socket closed

    def _retry_loop(self):
        """Resends un-ACKed messages periodically."""
        while self.running:
            time.sleep(0.1)
            now = time.time()
            # Create a copy of keys to modify dict safely
            for mid in list(self.pending_acks.keys()):
                item = self.pending_acks[mid]
                if now - item['time'] > RETRY_INTERVAL:
                    if item['retries'] < MAX_RETRIES:
                        # Resend
                        raw_bytes = json.dumps(item['packet']).encode("utf-8")
                        self.sock.sendto(raw_bytes, item['addr'])
                        item['time'] = now
                        item['retries'] += 1
                    else:
                        print(f"[System] Message {mid} failed to send.")
                        del self.pending_acks[mid]

    def _send_ack(self, msg_id, addr):
        ack = {"t": "ACK", "i": msg_id}
        self.sock.sendto(json.dumps(ack).encode("utf-8"), addr)

# --- GLOBAL STORE ---
# We need to map the user's raw socket object to our "ReliableState" manager
_socket_managers = {}

def _get_manager(sock: socket.socket) -> ReliableUDPState:
    """Singleton-like accessor to attach state to a socket."""
    if sock not in _socket_managers:
        _socket_managers[sock] = ReliableUDPState(sock)
    return _socket_managers[sock]

# --- YOUR REQUESTED FUNCTIONS ---

def send_message(sock: socket.socket, data: dict, addr: tuple[str, int]) -> None:
    """
    Sends a JSON dictionary reliably. Returns immediately (non-blocking),
    but retries in the background until an ACK is received.
    """
    manager = _get_manager(sock)
    
    msg_id = str(uuid.uuid4())[:8]
    packet = {
        "t": "DATA", 
        "i": msg_id, 
        "p": data  # The user's actual payload
    }
    
    # Store in pending buffer (this triggers the retry loop to start sending)
    manager.pending_acks[msg_id] = {
        'packet': packet,
        'addr': addr,
        'time': 0, # 0 forces immediate send by the thread
        'retries': 0
    }

def recv_message(sock: socket.socket) -> dict:
    """
    Blocks until a valid JSON message is received.
    Handles duplicates and ACKs automatically.
    """
    manager = _get_manager(sock)

    # Get the next message from the queue (blocking)
    payload, sender_addr = manager.incoming_queue.get()

    # Optional: If you need the sender address, you might want to return it too.
    # But to match your original signature, we just return the dict.
    return payload

def try_recv_message(sock: socket.socket) -> dict | None:
    """Non-blocking receive. Returns None if no message is available."""
    manager = _get_manager(sock)
    try:
        payload, _ = manager.incoming_queue.get_nowait()
        return payload
    except queue.Empty:
        return None