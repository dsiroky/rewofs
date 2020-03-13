/// Client side transport.
///
/// @file

#pragma once
#ifndef TRANSPORT_HPP__WALU1VCO
#define TRANSPORT_HPP__WALU1VCO

#include <thread>

#include "rewofs/disablewarnings.hpp"
#include "rewofs/enablewarnings.hpp"

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
} // namespace rewofs::client

#endif /* include guard */
