/// @copydoc heartbeat.hpp
///
/// @file

#include "rewofs/client/heartbeat.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

Heartbeat::Heartbeat(Serializer& serializer, Deserializer& deserializer,
                     BackgroundLoader& loader)
    : m_serializer{serializer}
    , m_deserializer{deserializer}
    , m_loader{loader}
{
}

//--------------------------------------------------------------------------

void Heartbeat::start()
{
    m_runner = std::thread{&Heartbeat::run, this};
}

//--------------------------------------------------------------------------

void Heartbeat::stop()
{
    m_quit = true;
}

//--------------------------------------------------------------------------

void Heartbeat::wait()
{
    if (m_runner.joinable())
    {
        m_runner.join();
    }
}

//--------------------------------------------------------------------------

void Heartbeat::run()
{
    log_info("starting heartbeat");
    while (not m_quit)
    {
        // currently only ad-hoc signal for the first connection
        flatbuffers::FlatBufferBuilder fbb{};
        const auto ping = messages::CreatePing(fbb);
        const auto mid = m_serializer.add_command(m_queue, fbb, ping);
        log_trace("mid:{}", strong::value_of(mid));
        const auto res = m_deserializer.wait_for_result<messages::Pong>(mid, TIMEOUT);
        if (res.is_valid())
        {
            if (not m_connected)
            {
                on_connect();
                m_connected = true;
            }
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
        else
        {
            if (m_connected)
            {
                on_disconnect();
                m_connected = false;
            }
        }
    }
}

//--------------------------------------------------------------------------

void Heartbeat::on_connect()
{
    log_info("connected");
    m_loader.invalidate_tree();
}

//--------------------------------------------------------------------------

void Heartbeat::on_disconnect()
{
    log_warning("disconnected");
}

//==========================================================================
} // namespace rewofs::client
