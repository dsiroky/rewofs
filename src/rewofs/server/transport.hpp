/// Server side transport.
///
/// @file

#pragma once
#ifndef TRANSPORT_HPP__RLOC8QB1
#define TRANSPORT_HPP__RLOC8QB1

#include <functional>
#include <string>

#include <gsl/span>

//==========================================================================
namespace rewofs::server {
//==========================================================================

class Transport
{
public:
    Transport();

    void set_endpoint(const std::string& endpoint);
    void send(const gsl::span<const uint8_t> buf);
    void recv(const std::function<void(const gsl::span<const uint8_t>)> cb);

private:
    int m_socket{-1};
};

//==========================================================================
} // namespace rewofs::server

#endif /* include guard */
