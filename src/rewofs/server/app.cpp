/// @copydoc server/app.hpp
///
/// @file

#include <csignal>

#include "rewofs/server/app.hpp"

//==========================================================================
namespace rewofs::server {
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
    g_app->m_worker.stop();
    g_app->m_watcher.stop();
    std::signal(SIGINT, SIG_DFL);
}

//==========================================================================

App::App(const boost::program_options::variables_map& options)
    : m_options{options}
{
    g_app = this;
}

//--------------------------------------------------------------------------

void App::run()
{
    const auto served_directory = m_options["serve"].as<std::string>();
    log_info("served directory request: {}", served_directory);
    boost::filesystem::current_path(served_directory);
    log_info("actual served directory: {}", boost::filesystem::current_path().native());

    const auto endpoint = m_options["listen"].as<std::string>();
    m_transport.set_endpoint(endpoint);

    std::signal(SIGINT, signal_handler);

    m_worker.start();
    m_watcher.start();
    m_watcher.wait();
    m_worker.wait();
}

//==========================================================================
} // namespace rewofs::server
