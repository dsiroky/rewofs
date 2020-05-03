/// Connection monitoring.
///
/// @file

#pragma once
#ifndef HEARTBEAT_HPP__FVUN2LKM
#define HEARTBEAT_HPP__FVUN2LKM

#include <thread>

#include "rewofs/transport.hpp"
#include "rewofs/client/vfs.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

class Heartbeat
{
public:
    Heartbeat(Serializer& serializer, Deserializer& deserializer, BackgroundLoader& loader);
    void start();
    void stop();
    void wait();

private:
    void run();
    void on_connect();
    void on_disconnect();

    Serializer& m_serializer;
    Deserializer& m_deserializer;
    BackgroundLoader& m_loader;
    std::thread m_runner{};
    std::atomic<bool> m_quit{false};
    Serializer::QueueRef m_queue{m_serializer.new_queue(Serializer::PRIORITY_HIGH)};
    bool m_connected{false};
};

//==========================================================================
} // namespace rewofs::client

#endif /* include guard */
