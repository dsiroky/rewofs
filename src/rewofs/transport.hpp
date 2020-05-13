/// Transport (network).
///
/// @file

#pragma once
#ifndef TRANSPORT_HPP__FEUC7AYM
#define TRANSPORT_HPP__FEUC7AYM

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "rewofs/disablewarnings.hpp"
#include <boost/noncopyable.hpp>
#include <flatbuffers/flatbuffers.h>
#include <gsl/span>
#include <strong_type/strong_type.hpp>
#include "rewofs/messages/all.hpp"
#include "rewofs/enablewarnings.hpp"

#include "rewofs/log.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

using MessageId
    = strong::type<uint64_t, struct MessageId_, strong::equality, strong::hashable>;

//==========================================================================

template<typename _Msg>
flatbuffers::Offset<messages::Frame> make_frame(flatbuffers::FlatBufferBuilder& fbb,
                                                const uint64_t id,
                                                flatbuffers::Offset<_Msg> offset)
{
    return messages::CreateFrame(fbb, id,
                                 messages::MessageTraits<_Msg>::enum_value,
                                 offset.Union());
}

//==========================================================================

/// Provides queues with priorities and outputs a single "stream" of messages.
/// Thread safe.
class Serializer : boost::noncopyable
{
public:
    /// Higher number means higher priority.
    using Priority = strong::type<uint8_t, struct Priority_,
                                  strong::default_constructible, strong::ordered>;
    class QueueRef;

    static constexpr Priority PRIORITY_BACKGROUND{0};
    static constexpr Priority PRIORITY_DEFAULT{10};
    static constexpr Priority PRIORITY_HIGH{100};

    /// @param seed for message ID's
    void set_msgid_seed(const uint64_t seed);

    QueueRef new_queue(const Priority priority);
    template<typename _Command>
    MessageId add_command(QueueRef& queue, flatbuffers::FlatBufferBuilder& fbb,
                          const flatbuffers::Offset<_Command> command);

    /// @return true if there is a message to be consumed.
    bool is_consumable() const;

    /// Consume a message. Does nothing if there is no pending message.
    /// @param callback called with a message content
    void pop(const std::function<void(const gsl::span<const uint8_t>)> callback);

    /// Wait until there is a message to be consumed.
    /// @return true if there is a message, false on timeout
    bool wait(const std::chrono::milliseconds timeout);

    //--------------------------------
private:
    struct Queue;

    std::atomic<uint64_t> m_id_dispenser{};
    mutable std::mutex m_mutex{};
    mutable std::mutex m_cv_mutex{};
    std::condition_variable m_cv{};
    std::list<Queue> m_queues{};

    using QueuesIt = decltype(Serializer::m_queues)::iterator;
    using CQueuesIt = decltype(Serializer::m_queues)::const_iterator;

    void drop_queue(const CQueuesIt it);
};

//--------------------------------------------------------------------------

struct Serializer::Queue
{
    Priority priority{};
    std::queue<std::vector<uint8_t>> queue{};

    Queue(const Queue&) = delete;
    Queue(Queue&&) = default;
};

//--------------------------------------------------------------------------

class Serializer::QueueRef : private boost::noncopyable
{
public:
    friend Serializer;

    ~QueueRef()
    {
        m_serializer.drop_queue(m_it);
    }

private:
    QueueRef(Serializer& serializer, const QueuesIt it)
        : m_serializer{serializer}, m_it{it}
    {
    }

private:
    Serializer& m_serializer;
    const QueuesIt m_it{};
};

//--------------------------------------------------------------------------

template<typename _Command>
MessageId Serializer::add_command(QueueRef& queue, flatbuffers::FlatBufferBuilder& fbb,
                                  const flatbuffers::Offset<_Command> command)
{
    const auto new_cmd_id = m_id_dispenser++;
    const auto frame = make_frame(fbb, new_cmd_id, command);
    fbb.Finish(frame);

    std::lock_guard lg{m_mutex};
    queue.m_it->queue.emplace(fbb.GetBufferPointer(),
                              fbb.GetBufferPointer() + fbb.GetSize());

    m_cv.notify_one();

    return MessageId{new_cmd_id};
}

//==========================================================================

/// Process incoming raw messages and call registered callbacks.
class Distributor
{
public:
    /// Subscribe a callback for a particular message.
    template<typename _Msg>
    void subscribe(std::function<void(const MessageId, const _Msg&)> callback);
    /// Process a raw message and call an appropriate callback.
    void process_frame(const gsl::span<const uint8_t> buf);

private:
    std::unordered_map<messages::Message, std::function<void(const gsl::span<const uint8_t>)>>
        m_subscriptions{};
};

//--------------------------------------------------------------------------

template<typename _Msg>
void Distributor::subscribe(std::function<void(const MessageId, const _Msg&)> callback)
{
    const auto command_type = messages::MessageTraits<_Msg>::enum_value;
    m_subscriptions.emplace(std::make_pair(
        command_type,
        [cb = std::move(callback)](const gsl::span<const uint8_t> buf) {
            const auto& frame = *flatbuffers::GetRoot<messages::Frame>(buf.data());
            cb(MessageId{frame.id()}, *frame.message_as<_Msg>());
        }));
}

//==========================================================================

/// Wait for replies with a particular message ID.
class Deserializer : private boost::noncopyable
{
public:
    template<typename _Msg>
    class Result;

    Deserializer();

    void process_frame(const gsl::span<const uint8_t> raw_frame);
    /// Wait for a specific message ID and type.
    /// @param mid Message ID of incoming response
    /// @param timeout
    /// @param callback will be called on response arrival
    /// @return true if the response was received, false on timeout
    template<typename _Msg>
    Result<_Msg> wait_for_result(const MessageId mid,
                                 const std::chrono::milliseconds timeout);

private:
    struct Item
    {
        std::chrono::steady_clock::time_point m_arrival{};
        std::vector<uint8_t> m_data{};
    };

    mutable std::mutex m_mutex{};
    std::condition_variable m_cv{};
    std::unordered_map<MessageId, Item> m_items{};
};

//--------------------------------------------------------------------------

template<typename _Msg>
class Deserializer::Result
{
public:
    /// Construct empty/invalid result.
    Result() {}

    Result(std::vector<uint8_t>&& raw_frame)
        : m_raw_frame{std::move(raw_frame)}
    {
    }

    bool is_valid() const
    {
        return m_raw_frame.size() > 0;
    }

    const _Msg& message() const
    {
        if (not is_valid())
        {
            throw std::runtime_error{"accessing invalid message"};
        }
        const auto frame = flatbuffers::GetRoot<messages::Frame>(m_raw_frame.data());
        return *frame->template message_as<_Msg>();
    }

private:
    std::vector<uint8_t> m_raw_frame{};
};

//--------------------------------------------------------------------------

template<typename _Msg>
Deserializer::Result<_Msg>
    Deserializer::wait_for_result(const MessageId mid,
                                  const std::chrono::milliseconds timeout)
{
    const auto deserialize_and_match
        = [mid](const gsl::span<const uint8_t> data)
        -> const messages::Frame*
        {
            if (not flatbuffers::Verifier(data.data(), data.size())
                        .VerifyBuffer<messages::Frame>())
            {
                return {};
            };
            const auto frame = flatbuffers::GetRoot<messages::Frame>(data.data());
            const auto requested_msg_type = messages::MessageTraits<_Msg>::enum_value;
            if ((frame->id() != strong::value_of(mid))
                or (requested_msg_type != frame->message_type()))
            {
                return {};
            }
            return frame;
        };

    std::unique_lock lg{m_mutex};
    log_trace("waiting for mid:{} for {}ms", strong::value_of(mid), timeout.count());

    const auto until = std::chrono::steady_clock::now() + timeout;
    do {
        const auto it = m_items.find(mid);
        if (it != m_items.end())
        {
            const auto frame
                = deserialize_and_match({it->second.m_data.data(), it->second.m_data.size()});
            if (frame != nullptr)
            {
                auto raw_frame = std::move(it->second.m_data);
                m_items.erase(it);
                return Result<_Msg>{std::move(raw_frame)};
            }
        }
        m_cv.wait_until(lg, until,
                        [this, mid]() { return m_items.find(mid) != m_items.end(); });
    } while (std::chrono::steady_clock::now() <= until);

    log_trace("timeout mid:{}", strong::value_of(mid));
    return {};
}

//==========================================================================
} // namespace rewofs

#endif /* include guard */
