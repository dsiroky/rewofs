/// @copydoc client/app.hpp
///
/// @file

#include <chrono>
#include <csignal>

#include "rewofs/client/app.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

namespace
{
App* g_app{};
}

//==========================================================================

void App::signal_handler(int)
{
    log_info("caught SIGINT, quiting");
    assert(g_app != nullptr);
    g_app->m_heartbeat.stop();
    g_app->m_fuse.stop();
    g_app->m_background_loader.stop();
    g_app->m_transport.stop();
    std::signal(SIGINT, SIG_DFL);
}

//==========================================================================

App::App(const boost::program_options::variables_map& options)
    : m_options{options}
{
    g_app = this;

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
    m_background_loader.start();
    m_heartbeat.start();
    m_fuse.start();

    std::signal(SIGINT, signal_handler);

    m_fuse.wait();
    m_heartbeat.wait();
    m_background_loader.wait();
    m_transport.wait();
}

//==========================================================================
} // namespace rewofs::client
