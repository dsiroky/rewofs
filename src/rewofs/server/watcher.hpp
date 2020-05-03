/// Watch FS for changes.
///
/// @file

#pragma once
#ifndef WATCHER_HPP__DNJ8BT5L
#define WATCHER_HPP__DNJ8BT5L

#include <thread>

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <flatbuffers/flatbuffers.h>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/server/transport.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

class Watcher : private boost::noncopyable
{
public:
    Watcher(server::Transport& transport);

    void start();
    void stop();
    void wait();

private:
    using Path = boost::filesystem::path;

    void run();

    template<typename _Msg>
    void send(flatbuffers::FlatBufferBuilder& fbb, flatbuffers::Offset<_Msg> msg);
    void notify_change();

    server::Transport& m_transport;
    std::thread m_thread{};
    std::atomic<bool> m_quit{false};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
