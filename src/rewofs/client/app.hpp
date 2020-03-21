/// Client entry point.
///
/// @file

#pragma once
#ifndef CLIENT_HPP__XH3TVUS8
#define CLIENT_HPP__XH3TVUS8

#include "rewofs/disablewarnings.hpp"
#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/client/fuse.hpp"
#include "rewofs/client/vfs.hpp"
#include "rewofs/client/transport.hpp"

//==========================================================================

namespace rewofs::client {

class App : private boost::noncopyable
{
public:
    App(const boost::program_options::variables_map& options);

    void run();

    //--------------------------------
private:
    const boost::program_options::variables_map& m_options;
    Serializer m_serializer{};
    Deserializer m_deserializer{};
    client::Transport m_transport{m_serializer, m_deserializer};
    RemoteVfs m_remote_vfs{m_serializer, m_deserializer};
    Fuse m_fuse{m_remote_vfs};
};

} // namespace rewofs::client

#endif /* include guard */
