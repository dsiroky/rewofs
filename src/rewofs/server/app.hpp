/// Server entry point.
///
/// @file

#pragma once
#ifndef APP_HPP__GJZIAT5Y
#define APP_HPP__GJZIAT5Y

#include "rewofs/disablewarnings.hpp"
#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/server/transport.hpp"
#include "rewofs/server/watcher.hpp"
#include "rewofs/server/worker.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

class App : private boost::noncopyable
{
public:
    App(const boost::program_options::variables_map& options);

    void run();

    //--------------------------------
private:
    static void signal_handler(int);

    const boost::program_options::variables_map& m_options;
    server::Transport m_transport{};
    TemporalIgnores m_temporal_ignores{std::chrono::seconds{1}};
    Watcher m_watcher{m_transport, m_temporal_ignores};
    Worker m_worker{m_transport, m_temporal_ignores};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
