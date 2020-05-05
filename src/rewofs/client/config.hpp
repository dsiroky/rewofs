/// Client static configuration.
///
/// @file

#pragma once
#ifndef CONFIG_HPP__CDJVISHN
#define CONFIG_HPP__CDJVISHN

#include <chrono>

//==========================================================================
namespace rewofs::client {
//==========================================================================

inline static constexpr std::chrono::seconds TIMEOUT{30};

//==========================================================================
} // namespace rewofs::client

#endif /* include guard */
