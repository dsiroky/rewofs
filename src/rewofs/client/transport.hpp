/// Client side transport.
///
/// @file

#pragma once
#ifndef TRANSPORT_HPP__WALU1VCO
#define TRANSPORT_HPP__WALU1VCO

#include <thread>

#include "rewofs/client/config.hpp"
#include "rewofs/transport.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

class Transport
{
public:
    Transport(Serializer& serializer, Deserializer& deserializer);

    void set_endpoint(const std::string& endpoint);
    void start();
    void stop();
    void wait();

private:
    void run_reader();
    void run_writer();

    Serializer& m_serializer;
    Deserializer& m_deserializer;
    int m_socket{-1};
    std::thread m_reader{};
    std::thread m_writer{};
    std::atomic<bool> m_quit{false};
};

//==========================================================================

/// Channel for single commands/responses.
class SingleComm
{
public:
    SingleComm(Serializer& serializer, Deserializer& deserializer);

    template<typename _Result, typename _Command>
    Deserializer::Result<_Result>
        single_command(flatbuffers::FlatBufferBuilder& fbb,
                       const flatbuffers::Offset<_Command> command,
                       const std::chrono::milliseconds timeout = TIMEOUT);

private:
    Serializer& m_serializer;
    Deserializer& m_deserializer;
};

//--------------------------------------------------------------------------

template<typename _Result, typename _Command>
Deserializer::Result<_Result>
    SingleComm::single_command(flatbuffers::FlatBufferBuilder& fbb,
                              const flatbuffers::Offset<_Command> command,
                              const std::chrono::milliseconds timeout)
{
    auto queue = m_serializer.new_queue(Serializer::PRIORITY_DEFAULT);
    const auto mid = m_serializer.add_command(queue, fbb, command);
    log_trace("mid:{}", strong::value_of(mid));
    const auto res = m_deserializer.wait_for_result<_Result>(mid, timeout);
    if (not res.is_valid())
    {
        throw std::system_error{EHOSTUNREACH, std::generic_category()};
    }
    return res;
}

//==========================================================================
} // namespace rewofs::client

#endif /* include guard */
