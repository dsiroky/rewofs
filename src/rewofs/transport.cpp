/// @copydoc transport.hpp
///
/// @file

#include "rewofs/log.hpp"
#include "rewofs/transport.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

flatbuffers::Offset<messages::CommandStat>
    make_command_stat(flatbuffers::FlatBufferBuilder& fbb, const std::string& path)
{
    auto fn = fbb.CreateString(path);
    return messages::CreateCommandStat(fbb, fn);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::CommandReaddir>
    make_command_readdir(flatbuffers::FlatBufferBuilder& fbb, const std::string& path)
{
    auto fn = fbb.CreateString(path);
    return messages::CreateCommandReaddir(fbb, fn);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::CommandReadlink>
    make_command_readlink(flatbuffers::FlatBufferBuilder& fbb, const std::string& path)
{
    auto fn = fbb.CreateString(path);
    return messages::CreateCommandReadlink(fbb, fn);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::CommandOpen>
    make_command_open(flatbuffers::FlatBufferBuilder& fbb, const std::string& path,
                      const int flags)
{
    auto fn = fbb.CreateString(path);
    return messages::CreateCommandOpen(fbb, fn, flags);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::CommandClose>
    make_command_close(flatbuffers::FlatBufferBuilder& fbb, const std::string& path)
{
    auto fn = fbb.CreateString(path);
    return messages::CreateCommandClose(fbb, fn);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultGeneric>
    make_result_generic(flatbuffers::FlatBufferBuilder& fbb, const int32_t status)
{
    return messages::CreateResultGeneric(fbb, status);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultRead>
    make_result_read(flatbuffers::FlatBufferBuilder& fbb, const int32_t status,
                     const gsl::span<const uint8_t> data)
{
    const auto vdata = fbb.CreateVector(data.data(), data.size());
    return messages::CreateResultRead(fbb, status, vdata);
}

//==========================================================================

Serializer::Serializer(const uint64_t msgid_seed)
    : m_id_dispenser{msgid_seed}
{
}

//--------------------------------------------------------------------------

Serializer::QueueRef Serializer::new_queue(const Priority priority)
{
    Queue q{};
    q.priority = priority;
    m_queues.emplace_back(std::move(q));
    return QueueRef{*this, std::prev(m_queues.end())};
}

//--------------------------------------------------------------------------

bool Serializer::is_consumable() const
{
    std::unique_lock lg{m_mutex};
    for (const auto& q : m_queues)
    {
        if (not q.queue.empty())
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------

void Serializer::pop(
    const std::function<void(const gsl::span<const uint8_t>)> callback)
{
    std::lock_guard lg{m_mutex};

    // naive solution for priority queues - pick from the queue with the highest priority
    // TODO round robin with prioritizing

    std::vector<QueuesIt> queues{};
    queues.reserve(m_queues.size());
    for (auto it = m_queues.begin(); it != m_queues.end(); ++it)
    {
        queues.emplace_back(it);
    }
    std::sort(queues.begin(), queues.end(), [](const auto it1, const auto it2) {
        return it2->priority < it1->priority;
    });

    for (auto it: queues)
    {
        auto& queue = it->queue;
        if (not queue.empty())
        {
            callback({queue.front().data(), queue.front().size()});
            queue.pop();
            break;
        }
    }
}

//--------------------------------------------------------------------------

bool Serializer::wait(const std::chrono::milliseconds timeout)
{
    if (is_consumable())
    {
        return true;
    }

    {
        std::unique_lock lg{m_cv_mutex};
        m_cv.wait_for(lg, timeout, [this]() { return is_consumable(); });
    }

    return is_consumable();
}

//--------------------------------------------------------------------------

void Serializer::drop_queue(const CQueuesIt it)
{
    std::lock_guard lg{m_mutex};
    m_queues.erase(it);
}

//==========================================================================

void Distributor::process_frame(const gsl::span<const uint8_t> buf)
{
    if (not flatbuffers::Verifier(buf.data(), buf.size())
                .VerifyBuffer<messages::Frame>())
    {
        return;
    };

    const auto& frame = *flatbuffers::GetRoot<messages::Frame>(buf.data());
    const auto it = m_subscriptions.find(frame.message_type());
    if (it != m_subscriptions.end())
    {
        it->second(buf);
    }
}

//==========================================================================

Deserializer::Deserializer()
{
}

//--------------------------------------------------------------------------

void Deserializer::process_frame(const gsl::span<const uint8_t> raw_frame)
{
    if (not flatbuffers::Verifier(raw_frame.data(), raw_frame.size())
                .VerifyBuffer<messages::Frame>())
    {
        return;
    };
    const auto& frame = *flatbuffers::GetRoot<messages::Frame>(raw_frame.data());

    std::unique_lock lg{m_mutex};
    m_items.emplace(std::make_pair(
        frame.id(), Item{std::chrono::steady_clock::now(),
                         {raw_frame.data(), raw_frame.data() + raw_frame.size()}}));
    m_cv.notify_all();
}

//==========================================================================
} // namespace rewofs
