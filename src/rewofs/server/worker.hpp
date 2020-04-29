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

//==========================================================================
namespace rewofs::server {
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
        std::unique_lock<std::mutex> lock{m_mutex};
        m_condition.wait(lock, [this] { return !m_queue.empty(); });
        T rc{std::move(m_queue.back())};
        m_queue.pop_back();
        return rc;
    }

private:
    std::mutex m_mutex{};
    std::condition_variable m_condition{};
    std::deque<T> m_queue{};
};

//==========================================================================

class Worker
{
public:
    Worker(server::Transport& transport);

    void start();
    void wait();

private:
    void run();

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

    /// @return <fd, file lock>, fd=-1 if not found
    std::pair<int, std::unique_lock<std::mutex>> get_file_descriptor(const uint64_t fh);

    struct File
    {
        int fd{};
        /// serialize file IO
        std::mutex mutex{};
    };

    Transport& m_transport;
    BlockingQueue<std::vector<uint8_t>> m_requests_queue{};
    std::array<std::thread, 50> m_threads{};
    Distributor m_distributor{};
    std::mutex m_mutex{};
    /// filehandle:file
    std::unordered_map<uint64_t, File> m_opened_files{};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
