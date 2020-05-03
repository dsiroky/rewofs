/// Watch FS for changes.
///
/// @file

#pragma once
#ifndef WATCHER_HPP__DNJ8BT5L
#define WATCHER_HPP__DNJ8BT5L

#include <chrono>
#include <mutex>
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

/// Map for ignoring remote actions.
class TemporalIgnores : private boost::noncopyable
{
private:
    using Path = boost::filesystem::path;

public:
    TemporalIgnores(const std::chrono::milliseconds ignore_duration);

    /// Add a temporal ignore.
    void add(const std::chrono::steady_clock::time_point now, Path path);
    /// @return true if the path should be ignored.
    bool check(const std::chrono::steady_clock::time_point now, const Path& path);

private:
    struct Item
    {
        std::chrono::steady_clock::time_point tp{};
        Path path{};
    };

    struct ItemsOrder
    {
        constexpr bool operator()(const Item& i1, const Item& i2) const
        {
            return i1.tp < i2.tp;
        }
    };

    const std::chrono::milliseconds m_ignore_duration{};
    std::deque<Item> m_items{};
    std::mutex m_mutex{};
};

//==========================================================================

class Watcher : private boost::noncopyable
{
public:
    Watcher(server::Transport& transport, TemporalIgnores& temporal_ignores);

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
    TemporalIgnores& m_temporal_ignores;
    std::thread m_thread{};
    std::atomic<bool> m_quit{false};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
