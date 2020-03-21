/// @copydoc log.hpp
///
/// @file

#include <spdlog/spdlog.h>

#include "rewofs/log.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

static std::shared_ptr<spdlog::logger> g_console{};

//==========================================================================

void log_init()
{
    g_console = spdlog::stdout_color_mt("console");
#ifdef NDEBUG
    spdlog::set_level(spdlog::level::warn);
#else
    spdlog::set_level(spdlog::level::trace);
#endif
    g_console->set_pattern("[%H:%M:%S.%e %L] %v");
}

//--------------------------------------------------------------------------

void log_raw(const LogLevel level, const std::string& msg)
{
    assert(g_console);
    switch (level)
    {
        case LogLevel::TRACE:
            g_console->trace(msg);
            break;
        case LogLevel::DEBUG:
            g_console->debug(msg);
            break;
        case LogLevel::INFO:
            g_console->info(msg);
            break;
        case LogLevel::WARNING:
            g_console->warn(msg);
            break;
        case LogLevel::ERROR:
            g_console->error(msg);
            break;
        case LogLevel::CRITICAL:
            g_console->critical(msg);
            break;
        default:
            break;
    }
}

//==========================================================================
} // namespace rewofs
