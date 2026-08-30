#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace kns {

    class SimulationEngine;

    /// A passive TCP listener bound to a node. When a SYN arrives at the
    /// node's address the listener accepts it, creates a fresh TCPSession,
    /// and sends back a SYN-ACK.
    ///
    /// Lifecycle:
    ///   1. Call SimulationEngine::startTCPListen(node) to create and register
    ///      a listener.
    ///   2. When a SYN arrives at that node, PacketReceivedEvent checks the
    ///      listener registry and calls TCPListener::accept(source, engine).
    ///   3. A new TCPSession is created; the handshake continues normally.
    ///
    /// A single listener can accept many sequential connections. To model a
    /// busy server that refuses new connections when at capacity, check
    /// getActiveConnections() >= getBacklog().
    class TCPListener {
    public:
        using ConnectionCallback = std::function<void(std::uint64_t session_id)>;

        explicit TCPListener(int node_id, int backlog = 128) noexcept
            : node_id_(node_id), backlog_(backlog) {}

        int getNodeId() const noexcept { return node_id_; }
        int getBacklog() const noexcept { return backlog_; }
        void setBacklog(int backlog) noexcept { backlog_ = backlog; }

        bool isListening() const noexcept { return listening_; }
        void setListening(bool v) noexcept { listening_ = v; }

        /// Number of currently open (not yet CLOSED) sessions accepted by
        /// this listener. Informational — not enforced unless you check it.
        int getActiveConnections() const noexcept {
            return static_cast<int>(active_sessions_.size());
        }

        /// Accept an incoming SYN from source_node: create a new TCPSession
        /// in the engine and start the server-side handshake.
        /// Returns the new session_id, or 0 if the listener is not active or
        /// the backlog is full.
        std::uint64_t accept(int source_node, SimulationEngine& engine);

        /// Track an accepted session so the listener can report active count.
        void trackSession(std::uint64_t session_id) {
            active_sessions_.push_back(session_id);
        }

        /// Remove a session from the active list (called when CLOSED).
        void untrackSession(std::uint64_t session_id) {
            active_sessions_.erase(
                std::remove(active_sessions_.begin(), active_sessions_.end(), session_id),
                active_sessions_.end());
        }

        /// Optional callback invoked after each accepted connection.
        void setOnAccept(ConnectionCallback cb) { on_accept_ = std::move(cb); }

    private:
        int node_id_;
        int backlog_;
        bool listening_ = true;
        std::vector<std::uint64_t> active_sessions_;
        ConnectionCallback on_accept_;
    };

} // namespace kns
