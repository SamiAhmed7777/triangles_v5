// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_SIGNAL_H
#define TRIANGLES_SIGNAL_H

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

/**
 * Lightweight multi-listener signal built on std::function.
 *
 * Two variants via partial specialization on a function signature:
 *   CSignal<void(Args...)>    fan-out signal, operator() returns void.
 *   CSignal<R(Args...)>       returns std::optional<R> from the most-recently
 *                             connected slot (mirroring signals2's last_value
 *                             policy with optional fallback when no slots).
 *
 * connect(slot) returns a Connection token; call .disconnect() to unsubscribe.
 *
 * Disconnect-by-bind-identity (signals2's compare-by-equivalent-bind trick) is
 * intentionally not provided — std::function lacks equality. Callers should
 * keep the Connection token (or a CSignalConnections bag) and disconnect via it.
 */
template <typename Signature> class CSignal;

namespace signal_detail {

template <typename Slot>
struct SignalState
{
    std::map<std::size_t, Slot> slots;
    std::size_t next_id = 0;
};

template <typename Slot>
class ConnectionT
{
public:
    ConnectionT() = default;

    void disconnect()
    {
        if (auto sp = m_state.lock()) {
            sp->slots.erase(m_id);
        }
        m_state.reset();
    }

    template <typename> friend class SignalBase;

    ConnectionT(std::weak_ptr<SignalState<Slot>> state, std::size_t id)
        : m_state(std::move(state)), m_id(id) {}

private:
    std::weak_ptr<SignalState<Slot>> m_state;
    std::size_t m_id = 0;
};

template <typename Slot>
class SignalBase
{
public:
    using slot_type = Slot;
    using Connection = ConnectionT<Slot>;

    SignalBase() : m_state(std::make_shared<SignalState<Slot>>()) {}
    SignalBase(const SignalBase&) = delete;
    SignalBase& operator=(const SignalBase&) = delete;

    Connection connect(slot_type slot)
    {
        std::size_t id = ++m_state->next_id;
        m_state->slots.emplace(id, std::move(slot));
        return Connection(m_state, id);
    }

    bool empty() const { return m_state->slots.empty(); }

protected:
    std::shared_ptr<SignalState<Slot>> m_state;
};

} // namespace signal_detail

// Void-returning specialization: invoke every connected slot.
template <typename... Args>
class CSignal<void(Args...)> : public signal_detail::SignalBase<std::function<void(Args...)>>
{
public:
    template <typename... CallArgs>
    void operator()(CallArgs&&... args) const
    {
        // Snapshot lets slots mutate connections during invocation.
        auto snapshot = this->m_state->slots;
        for (auto& kv : snapshot) {
            if (kv.second) kv.second(args...);
        }
    }
};

// Non-void specialization: invoke every slot, return the most-recent slot's
// result wrapped in optional (empty if no slots are connected).
template <typename R, typename... Args>
class CSignal<R(Args...)> : public signal_detail::SignalBase<std::function<R(Args...)>>
{
public:
    template <typename... CallArgs>
    std::optional<R> operator()(CallArgs&&... args) const
    {
        auto snapshot = this->m_state->slots;
        std::optional<R> result;
        for (auto& kv : snapshot) {
            if (kv.second) result = kv.second(args...);
        }
        return result;
    }
};

/**
 * Holder for a heterogeneous list of signal connections that should all be
 * released together (typical pattern: subscribe in ctor, drop on dtor).
 */
class CSignalConnections
{
public:
    CSignalConnections() = default;
    CSignalConnections(const CSignalConnections&) = delete;
    CSignalConnections& operator=(const CSignalConnections&) = delete;

    template <typename Conn>
    void add(Conn conn)
    {
        auto holder = std::make_shared<Conn>(std::move(conn));
        m_disconnectors.emplace_back([holder]() { holder->disconnect(); });
    }

    void disconnect_all()
    {
        for (auto& d : m_disconnectors) d();
        m_disconnectors.clear();
    }

    ~CSignalConnections() { disconnect_all(); }

private:
    std::vector<std::function<void()>> m_disconnectors;
};

#endif // TRIANGLES_SIGNAL_H
