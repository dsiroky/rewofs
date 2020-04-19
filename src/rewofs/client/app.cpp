/// @copydoc client/app.hpp
///
/// @file

#include <chrono>

#include "rewofs/client/app.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

App::App(const boost::program_options::variables_map& options)
    : m_options{options}
{
    const auto seed = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    m_serializer.set_msgid_seed(seed);
    m_id_dispenser.set_seed(seed);
}

//--------------------------------------------------------------------------

void App::run()
{
    const auto endpoint = m_options["connect"].as<std::string>();
    m_transport.set_endpoint(endpoint);
    const auto mountpoint = m_options["mountpoint"].as<std::string>();
    m_fuse.set_mountpoint(mountpoint);

    m_transport.start();
    m_heartbeat.start();
    m_fuse.start();
    m_fuse.wait();
    m_heartbeat.stop();
    m_heartbeat.wait();
    m_transport.stop();
    m_transport.wait();
}

//==========================================================================
} // namespace rewofs::client
