/// @copydoc client/transport.hpp
///
/// @file

#include <nanomsg/nn.h>
#include <nanomsg/pair.h>

#include "rewofs/client/transport.hpp"
#include "rewofs/log.hpp"
#include "rewofs/nanomsg.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

Transport::Transport(Serializer& serializer, Deserializer& deserializer)
    : m_serializer{serializer}
    , m_deserializer{deserializer}
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
    log_info("remote endpoint: {}", endpoint);
    nanomsg::check("nn_connect", nn_connect(m_socket, endpoint.c_str()));
}

//--------------------------------------------------------------------------

void Transport::start()
{
    m_reader = std::thread{&Transport::run_reader, this};
    m_writer = std::thread{&Transport::run_writer, this};
}

//--------------------------------------------------------------------------

void Transport::stop()
{
    m_quit = true;
}

//--------------------------------------------------------------------------

void Transport::wait()
{
    if (m_reader.joinable())
    {
        m_reader.join();
    }
    if (m_writer.joinable())
    {
        m_writer.join();
    }
}

//--------------------------------------------------------------------------

void Transport::run_reader()
{
    log_info("starting reader");
    while (not m_quit)
    {
        nanomsg::receive(m_socket, [this](const gsl::span<const uint8_t> buf) {
            m_deserializer.process_frame(buf);
        });
    }
}

//--------------------------------------------------------------------------

void Transport::run_writer()
{
    log_info("starting writer");
    while (not m_quit)
    {
        while (m_serializer.is_consumable())
        {
            m_serializer.pop([this](const auto buf) {
                nn_send(m_socket, buf.data(), buf.size(), 0);
            });
        }
        m_serializer.wait(std::chrono::milliseconds{100});
    }
}

//==========================================================================
} // namespace rewofs::client
