/// Process client requests.
///
/// @file

#pragma once
#ifndef WORKER_HPP__AJHLUD1B
#define WORKER_HPP__AJHLUD1B

#include <thread>
#include <unordered_map>

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/transport.hpp"
#include "rewofs/server/transport.hpp"
#include "rewofs/server/watcher.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

struct QuitSignal {};

//==========================================================================

template<typename T>
class BlockingQueue
{
public:

    void push(T const& value)
    {
        std::unique_lock<std::mutex> lock{m_mutex};
        m_queue.push_front(value);
        m_condition.notify_one();
    }

    T pop()
    {
        if (m_stopped)
        {
            throw QuitSignal{};
        }
        std::unique_lock<std::mutex> lock{m_mutex};
        m_condition.wait(lock, [this] { return not m_queue.empty() or m_stopped; });
        if (m_stopped)
        {
            throw QuitSignal{};
        }
        T rc{std::move(m_queue.back())};
        m_queue.pop_back();
        return rc;
    }

    void stop()
    {
        m_stopped = true;
        m_condition.notify_all();
    }

private:
    std::mutex m_mutex{};
    std::condition_variable m_condition{};
    std::deque<T> m_queue{};
    std::atomic<bool> m_stopped{false};
};

//==========================================================================

class Worker
{
public:
    Worker(server::Transport& transport, TemporalIgnores& temporal_ignores);

    void start();
    void stop();
    void wait();

private:
    struct File
    {
        int fd{};
        std::string path{};
        /// serialize file IO
        std::mutex mutex{};
    };

    struct FileRef
    {
        int fd{};
        std::optional<std::reference_wrapper<std::string>> path{};

        bool is_valid() const { return fd >= 0; }
    };

    void recv_loop();
    void run();
    void temporal_ignore(const std::string& path);

    template<typename _Msg, typename _ProcFunc>
    void process_message(const MessageId mid, const _Msg& msg, _ProcFunc proc);
    flatbuffers::Offset<messages::Pong>
        process_ping(flatbuffers::FlatBufferBuilder& fbb,
                     const messages::Ping&);
    flatbuffers::Offset<messages::ResultReadTree>
        process_read_tree(flatbuffers::FlatBufferBuilder& fbb,
                     const messages::CommandReadTree& msg);
    flatbuffers::Offset<messages::ResultStat>
        process_stat(flatbuffers::FlatBufferBuilder& fbb,
                     const messages::CommandStat& msg);
    flatbuffers::Offset<messages::ResultReaddir>
        process_readdir(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandReaddir& msg);
    flatbuffers::Offset<messages::ResultReadlink>
        process_readlink(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandReadlink& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_mkdir(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandMkdir& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_rmdir(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandRmdir& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_unlink(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandUnlink& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_symlink(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandSymlink& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_rename(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandRename& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_chmod(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandChmod& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_utime(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandUtime& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_truncate(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandTruncate& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_open(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandOpen& msg);
    flatbuffers::Offset<messages::ResultErrno>
        process_close(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandClose& msg);
    flatbuffers::Offset<messages::ResultRead>
        process_read(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandRead& msg);
    flatbuffers::Offset<messages::ResultWrite>
        process_write(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandWrite& msg);

    std::pair<FileRef, std::unique_lock<std::mutex>> get_file_descriptor(const uint64_t fh);

    Transport& m_transport;
    TemporalIgnores& m_temporal_ignores;
    std::atomic<bool> m_quit{false};
    BlockingQueue<std::vector<uint8_t>> m_requests_queue{};
    std::thread m_recv_thread{};
    std::array<std::thread, 50> m_threads{};
    Distributor m_distributor{};
    std::mutex m_mutex{};
    /// filehandle:file
    std::unordered_map<uint64_t, File> m_opened_files{};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
