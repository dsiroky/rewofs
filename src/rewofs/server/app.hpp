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
    Watcher m_watcher{m_transport};
    Worker m_worker{m_transport};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
