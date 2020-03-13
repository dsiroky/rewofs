/// Process client requests.
///
/// @file

#pragma once
#ifndef WORKER_HPP__AJHLUD1B
#define WORKER_HPP__AJHLUD1B

#include <thread>

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
    flatbuffers::Offset<messages::ResultMkdir>
        process_mkdir(flatbuffers::FlatBufferBuilder& fbb,
                        const messages::CommandMkdir& msg);

    Transport& m_transport;
    std::thread m_thread{};
    Distributor m_distributor{};
    boost::filesystem::path m_served_directory{};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
