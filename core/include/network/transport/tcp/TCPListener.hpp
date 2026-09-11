#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_set>
#include <utility>

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

        static constexpr std::uint64_t INVALID_SESSION_ID =
            std::numeric_limits<std::uint64_t>::max();

        explicit TCPListener(int node_id, int backlog = 128) noexcept
            : node_id_(node_id), backlog_(backlog) {}

        int getNodeId() const noexcept { return node_id_; }
        int getBacklog() const noexcept { return backlog_; }
        /// A non-positive backlog permits an unlimited number of connections.
        void setBacklog(int backlog) noexcept { backlog_ = backlog; }

        bool isListening() const noexcept { return listening_; }
        void setListening(bool v) noexcept { listening_ = v; }

        /// Number of sessions currently tracked by this listener.
        std::size_t getActiveConnections() const noexcept {
            return active_sessions_.size();
        }

        /// Accept an incoming SYN from source_node: create a new TCPSession
        /// in the engine and start the server-side handshake.
        /// Returns the new session_id, or INVALID_SESSION_ID if the listener
        /// is not active or the backlog is full.
        std::uint64_t accept(
            int source_node,
            std::uint32_t source_seq,
            SimulationEngine& engine
        );

        /// Track an accepted session so the listener can report active count.
        /// Repeated tracking of the same ID has no effect.
        void trackSession(std::uint64_t session_id) {
            active_sessions_.insert(session_id);
        }

        /// Remove a session from the active registry (called when CLOSED).
        void untrackSession(std::uint64_t session_id) {
            active_sessions_.erase(session_id);
        }

        /// Optional callback invoked after each accepted connection.
        void setOnAccept(ConnectionCallback cb) { on_accept_ = std::move(cb); }

    private:
        bool isBacklogFull() const noexcept {
            return backlog_ > 0 &&
                active_sessions_.size() >=
                    static_cast<std::size_t>(backlog_);
        }

        int node_id_;
        int backlog_;
        bool listening_ = true;
        std::unordered_set<std::uint64_t> active_sessions_;
        ConnectionCallback on_accept_;
    };

} // namespace kns
