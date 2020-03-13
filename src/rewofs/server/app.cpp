/// @copydoc server/app.hpp
///
/// @file

#include "rewofs/server/app.hpp"

#include <boost/filesystem.hpp>

//==========================================================================
namespace rewofs::server {
//==========================================================================

App::App(const boost::program_options::variables_map& options)
    : m_options{options}
{
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

    m_worker.start();
    m_worker.wait();
}

//==========================================================================
} // namespace rewofs::server
