/// @copydoc server/transport.hpp
///
/// @file

#include <nanomsg/nn.h>
#include <nanomsg/pair.h>

#include "rewofs/log.hpp"
#include "rewofs/nanomsg.hpp"
#include "rewofs/server/transport.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

Transport::Transport()
{
    m_socket = nanomsg::check("nn_socket", nn_socket(AF_SP, NN_PAIR));

    int tout{100};
    nanomsg::check("nn_setsockopt", nn_setsockopt(m_socket, NN_SOL_SOCKET, NN_RCVTIMEO,
                                                  &tout, sizeof(tout)));
    tout = 5000;
    nanomsg::check("nn_setsockopt", nn_setsockopt(m_socket, NN_SOL_SOCKET, NN_SNDTIMEO,
                                                  &tout, sizeof(tout)));

    int bufsize{10 * 1024 * 1024};
    nanomsg::check("nn_setsockopt", nn_setsockopt(m_socket, NN_SOL_SOCKET, NN_RCVBUF,
                                                  &bufsize, sizeof(bufsize)));
    nanomsg::check("nn_setsockopt", nn_setsockopt(m_socket, NN_SOL_SOCKET, NN_SNDBUF,
                                                  &bufsize, sizeof(bufsize)));
    int msgsize{1 * 1024 * 1024};
    nanomsg::check("nn_setsockopt", nn_setsockopt(m_socket, NN_SOL_SOCKET, NN_RCVMAXSIZE,
                                                  &msgsize, sizeof(msgsize)));
}

//--------------------------------------------------------------------------

void Transport::set_endpoint(const std::string& endpoint)
{
    log_info("local endpoint: {}", endpoint);
    nanomsg::check("nn_bind", nn_bind(m_socket, endpoint.c_str()));
}

//--------------------------------------------------------------------------

void Transport::send(const gsl::span<const uint8_t> buf)
{
    nn_send(m_socket, buf.data(), buf.size(), 0);
}

//--------------------------------------------------------------------------

void Transport::recv(const std::function<void(const gsl::span<const uint8_t>)> cb)
{
    nanomsg::receive(m_socket, [cb](const gsl::span<const uint8_t> buf) {
        cb(buf);
    });
}

//==========================================================================
} // namespace rewofs::server
