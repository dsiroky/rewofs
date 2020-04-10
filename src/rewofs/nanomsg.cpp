/// @copydoc nanomsg.hpp
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <fmt/format.h>
#include <nanomsg/nn.h>
#include <boost/scope_exit.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/nanomsg.hpp"

//==========================================================================
namespace rewofs::nanomsg {
//==========================================================================

/// If nanomsg function return value is < 0 then throws exception.
int check(const std::string& message, const int ret)
{
    if (ret < 0)
    {
        throw std::runtime_error{fmt::format("{}: {} (ret:{} errno:{})", message,
                                             nn_strerror(nn_errno()), ret, nn_errno())};
    }
    return ret;
}

//--------------------------------------------------------------------------

void receive(int sock, const std::function<void(const gsl::span<const uint8_t>)> recv_cb)
{
    char* buf{nullptr};
#include "rewofs/disablewarnings.hpp"
    const int recv_len{nn_recv(sock, &buf, NN_MSG, 0)};
#include "rewofs/enablewarnings.hpp"
    if (recv_len >= 0)
    {
        BOOST_SCOPE_EXIT_ALL(&buf) { nn_freemsg(buf); };
        recv_cb({reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(recv_len)});
    }
    // EINTR is caused e.g. by CTRL+C
    else if ((nn_errno() != EAGAIN) and (nn_errno() != ETIMEDOUT)
             and (nn_errno() != EINTR))
    {
        check("nn_recv", recv_len);
    }
}

//==========================================================================
} // namespace rewofs::nanomsg
