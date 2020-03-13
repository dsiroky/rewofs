/// @copydoc client/app.hpp
///
/// @file

#include "rewofs/client/app.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

App::App(const boost::program_options::variables_map& options)
    : m_options{options}
{
}

//--------------------------------------------------------------------------

void App::run()
{
    const auto endpoint = m_options["connect"].as<std::string>();
    m_transport.set_endpoint(endpoint);
    const auto mountpoint = m_options["mountpoint"].as<std::string>();
    m_fuse.set_mountpoint(mountpoint);

    m_transport.start();
    m_fuse.start();
    m_fuse.wait();
    m_transport.stop();
    m_transport.wait();
}

//==========================================================================
} // namespace rewofs::client
