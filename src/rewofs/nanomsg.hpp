/// Nanomsg helpers.
///
/// @file

#pragma once
#ifndef NANOMSG_HPP__DMT8EN9B
#define NANOMSG_HPP__DMT8EN9B

#include <functional>

#include <gsl/span>

//==========================================================================
namespace rewofs::nanomsg {
//==========================================================================

/// If nanomsg function return value is < 0 then throws exception.
int check(const std::string& message, const int ret);

/// Call nn_recv() and on success calls recv_cb. On error throws exception.
void receive(int sock, const std::function<void(const gsl::span<const uint8_t>)> recv_cb);

//==========================================================================
} // namespace rewofs::nanomsg

#endif /* include guard */
