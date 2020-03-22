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

class Worker
{
public:
    Worker(server::Transport& transport);

    void start();
    void wait();

private:
    void run();

    /// @return a host path from a relative path to the served directory
    static boost::filesystem::path map_path(const boost::filesystem::path& relative);

    template<typename _Msg, typename _ProcFunc>
    void process_message(const MessageId mid, const _Msg& msg, _ProcFunc proc);
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

    /// @return -1 if not found
    int get_file_descriptor(const uint64_t fh);

    Transport& m_transport;
    std::thread m_thread{};
    Distributor m_distributor{};
    boost::filesystem::path m_served_directory{};
    std::mutex m_mutex{};
    /// filehandle:file descriptor
    std::unordered_map<uint64_t, int> m_opened_files{};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
