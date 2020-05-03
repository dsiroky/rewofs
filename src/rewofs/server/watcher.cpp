/// @copydoc watcher.hpp
///
/// @file

#include <chrono>

#include "rewofs/disablewarnings.hpp"
#include <boost/scope_exit.hpp>
#include <inotifytools/inotifytools.h>
#include <inotifytools/inotify.h>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/log.hpp"
#include "rewofs/messages.hpp"
#include "rewofs/path.hpp"
#include "rewofs/server/watcher.hpp"
#include "rewofs/transport.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

namespace fs = boost::filesystem;

//==========================================================================

Watcher::Watcher(server::Transport& transport)
    : m_transport{transport}
{
}

//--------------------------------------------------------------------------

void Watcher::start()
{
    m_thread = std::thread{&Watcher::run, this};
}

//--------------------------------------------------------------------------

void Watcher::stop()
{
    m_quit = true;
}

//--------------------------------------------------------------------------

void Watcher::wait()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

//--------------------------------------------------------------------------

void Watcher::run()
{
    log_info("watcher start");

    static const std::string WATCH_PATH{"."};
    static constexpr auto EVENTS = IN_MODIFY | IN_ATTRIB | IN_MOVED_FROM | IN_MOVED_TO
                            | IN_CREATE | IN_DELETE | IN_DONT_FOLLOW;

    while (not m_quit)
    {
        // First do a fast inotify monitoring and then slow fingerprinting.
        // inotifytools_watch_recursively() does not automatically watch new entries.

        if (not inotifytools_initialize())
        {
            log_critical("{}", strerror(inotifytools_error()));
            return;
        }
        BOOST_SCOPE_EXIT_ALL()
        {
            inotifytools_cleanup();
        };
        if (not inotifytools_watch_recursively(WATCH_PATH.c_str(), EVENTS))
        {
            log_critical("{}", strerror(inotifytools_error()));
            break;
        }
        inotify_event* event{nullptr};
        do
        {
            event = inotifytools_next_event(1);
        } while (not m_quit and ((event == nullptr) or (event->mask & IN_IGNORED)));
        if (m_quit)
        {
            break;
        }

        const auto normalized
            = (Path{"/"} / inotifytools_filename_from_wd(event->wd) / event->name)
                  .lexically_normal();
        log_trace("inotify '{}' {}", normalized.native(), event->mask);
        //inotifytools_printf( event, "%T %w%f %e\n" );

        // Local modifications may take some time. Fingerprint in a loop and
        // notify after it is "stabilized".
        std::vector<BreadthDirectoryItem> fingerprint{};
        while (not m_quit)
        {
            try
            {
                auto new_fingerprint = breadth_first_tree(WATCH_PATH);
                if (fingerprint == new_fingerprint)
                {
                    log_trace("stabilized");
                    break;
                }
                fingerprint = std::move(new_fingerprint);
            }
            catch (const fs::filesystem_error& err)
            {
                // ignore errors caused by on-the-fly local tree modifications
                if (err.code() != boost::system::errc::no_such_file_or_directory)
                {
                    throw;
                }

            }
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
        if (m_quit)
        {
            break;
        }

        notify_change();
    }

    log_info("watcher done");
}

//--------------------------------------------------------------------------

template<typename _Msg>
void Watcher::send(flatbuffers::FlatBufferBuilder& fbb, flatbuffers::Offset<_Msg> msg)
{
    const auto frame = make_frame(fbb, {}, msg);
    fbb.Finish(frame);
    m_transport.send({fbb.GetBufferPointer(), fbb.GetSize()});
}

//--------------------------------------------------------------------------

void Watcher::notify_change()
{
    flatbuffers::FlatBufferBuilder fbb{};
    messages::NotifyChangedBuilder builder{fbb};
    send(fbb, builder.Finish());
}

//==========================================================================
} // namespace rewofs::server
