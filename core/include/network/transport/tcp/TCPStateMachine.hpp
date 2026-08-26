#pragma once

#include "enums/TCPState.hpp"

namespace kns {

    class TCPStateMachine {
        public:
            explicit TCPStateMachine(TCPState initial = TCPState::CLOSED) noexcept
                : state_(initial) {}

            TCPState state() const noexcept {
                return state_;
            }

            bool onSynSent() noexcept {
                if (state_ == TCPState::CLOSED || state_ == TCPState::LISTEN) {
                    state_ = TCPState::SYN_SENT;
                    return true;
                }
                return false;
            }

            bool onSynReceived() noexcept {
                if (state_ == TCPState::CLOSED ||
                    state_ == TCPState::LISTEN ||
                    state_ == TCPState::SYN_SENT) {
                    state_ = TCPState::SYN_RECEIVED;
                    return true;
                }
                return false;
            }

            bool onEstablished() noexcept {
                if (state_ == TCPState::SYN_SENT ||
                    state_ == TCPState::SYN_RECEIVED ||
                    state_ == TCPState::FIN_WAIT_1 ||
                    state_ == TCPState::FIN_WAIT_2 ||
                    state_ == TCPState::CLOSE_WAIT) {
                    state_ = TCPState::ESTABLISHED;
                    return true;
                }
                return false;
            }

            bool onFinSent() noexcept {
                if (state_ == TCPState::ESTABLISHED) {
                    state_ = TCPState::FIN_WAIT_1;
                    return true;
                }

                if (state_ == TCPState::CLOSE_WAIT) {
                    state_ = TCPState::LAST_ACK;
                    return true;
                }

                return false;
            }

            bool onFinAcked() noexcept {
                if (state_ == TCPState::FIN_WAIT_1) {
                    state_ = TCPState::FIN_WAIT_2;
                    return true;
                }

                if (state_ == TCPState::LAST_ACK) {
                    state_ = TCPState::CLOSED;
                    return true;
                }

                return false;
            }

            bool onPeerFin() noexcept {
                if (state_ == TCPState::ESTABLISHED) {
                    state_ = TCPState::CLOSE_WAIT;
                    return true;
                }

                if (state_ == TCPState::FIN_WAIT_1) {
                    state_ = TCPState::CLOSING;
                    return true;
                }

                if (state_ == TCPState::FIN_WAIT_2) {
                    state_ = TCPState::TIME_WAIT;
                    return true;
                }

                return false;
            }

            bool onTimeWaitDone() noexcept {
                if (state_ == TCPState::TIME_WAIT) {
                    state_ = TCPState::CLOSED;
                    return true;
                }
                return false;
            }

        private:
            TCPState state_;
    };
}
