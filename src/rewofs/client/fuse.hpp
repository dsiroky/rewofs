/// FUSE interface.
///
/// @file

#pragma once
#ifndef FUSE_HPP__EFM7PVEH
#define FUSE_HPP__EFM7PVEH

#include <thread>

#include "rewofs/disablewarnings.hpp"
#include <boost/noncopyable.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/client/vfs.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

class Fuse : private boost::noncopyable
{
public:
    Fuse(IVfs& vfs);

    void set_mountpoint(const std::string& path);
    void start();
    void wait();

private:
    void run();

    IVfs& m_vfs;

    std::string m_mountpoint{};
    std::thread m_thread{};
};

//==========================================================================
} // namespace rewofs::client

#endif /* include guard */
