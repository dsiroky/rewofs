/// FUSE interface.
///
/// @file

#pragma once
#ifndef FUSE_HPP__EFM7PVEH
#define FUSE_HPP__EFM7PVEH

#include <thread>

#include "rewofs/disablewarnings.hpp"
#include <boost/noncopyable.hpp>
#define FUSE_USE_VERSION 32
#include <fuse3/fuse.h>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/client/vfs.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

class Fuse : private boost::noncopyable
{
public:
    Fuse(IVfs& vfs);

    Fuse(const Fuse& vfs) = delete;
    Fuse& operator=(const Fuse& vfs) = delete;

    void set_mountpoint(const std::string& path);
    void start();
    void stop();
    void wait();

private:
    void run();

    IVfs& m_vfs;

    std::string m_mountpoint{};
    std::thread m_thread{};
    fuse* m_fuse{};
};

//==========================================================================
} // namespace rewofs::client

#endif /* include guard */
