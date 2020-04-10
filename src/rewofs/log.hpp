/// Logging.
///
/// @file

#pragma once
#ifndef LOG_HPP__KQWESS9N
#define LOG_HPP__KQWESS9N

#include "rewofs/disablewarnings.hpp"
#include <fmt/format.h>
#include "rewofs/enablewarnings.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

enum class LogLevel
{
  TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL
};

void log_init(const std::string& prefix);

/// spdlog takes incredible amount of time to compile. These are speedup
/// wrappers.
/// https://github.com/Timmmm/SpdlogBench
void log_raw(const LogLevel level, const std::string& msg);

inline const char* _log_filename_prefix(const std::string_view& path)
{
    constexpr std::string_view SRC_PREFIX{"src/rewofs/"};
    const auto prefix = path.find(SRC_PREFIX);
    if (prefix != std::string_view::npos)
    {
        return path.substr(prefix + SRC_PREFIX.size()).data();
    }

    return path.data();
}

#define _CODE_LOC \
    (fmt::format("{}:{}:{}(): ", rewofs::_log_filename_prefix(__FILE__), __LINE__, \
                 __func__))

/// parameters are for `fmt::format()`
#define log_trace(...) \
  rewofs::log_raw(rewofs::LogLevel::TRACE, _CODE_LOC + fmt::format(__VA_ARGS__))
/// parameters are for `fmt::format()`
#define log_debug(...) \
  rewofs::log_raw(rewofs::LogLevel::DEBUG, _CODE_LOC + fmt::format(__VA_ARGS__))
/// parameters are for `fmt::format()`
#define log_info(...) \
  rewofs::log_raw(rewofs::LogLevel::INFO, _CODE_LOC + fmt::format(__VA_ARGS__))
/// parameters are for `fmt::format()`
#define log_warning(...) \
  rewofs::log_raw(rewofs::LogLevel::WARNING, _CODE_LOC + fmt::format(__VA_ARGS__))
/// parameters are for `fmt::format()`
#define log_error(...) \
  rewofs::log_raw(rewofs::LogLevel::ERROR, _CODE_LOC + fmt::format(__VA_ARGS__))
/// parameters are for `fmt::format()`
#define log_critical(...) \
  rewofs::log_raw(rewofs::LogLevel::CRITICAL, _CODE_LOC + fmt::format(__VA_ARGS__))

//==========================================================================
} // namespace rewofs

#endif /* include guard */
