/// @copydoc fuse.hpp
///
/// @file

#include <stdexcept>

#define FUSE_USE_VERSION 32
#include <fuse3/fuse.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "rewofs/log.hpp"
#include "rewofs/transport.hpp"
#include "rewofs/client/fuse.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

namespace fs = boost::filesystem;

//==========================================================================

static struct fuse_operations g_oper{};
static IVfs* g_vfs{};

//==========================================================================

int gen_return_error_code()
{
    try
    {
        throw;
    }
    catch (const std::system_error& err)
    {
        assert(err.code().value() > 0);
        return -err.code().value();
    }
    catch (const std::exception& err)
    {
        log_error("{}", err.what());
    }

    return -EIO;
}

//==========================================================================
namespace callbacks {
//==========================================================================

static int getattr(const char* path, struct stat* stbuf, struct fuse_file_info*) noexcept
{
    log_trace("path:{}", path);
    try
    {
        g_vfs->getattr(path, *stbuf);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t,
                   struct fuse_file_info*, fuse_readdir_flags) noexcept
{
    log_trace("path:{}", path);
    try
    {
        g_vfs->readdir(path,
                       [buf, filler](const fs::path& item_path, const struct stat& st) {
                           log_trace("{}", item_path.c_str());
                           filler(buf, item_path.c_str(), &st, 0, FUSE_FILL_DIR_PLUS);
                       });
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int readlink(const char* path, char* dst, size_t sz) noexcept
{
    log_trace("path:{}", path);
    try
    {
        const auto link_path = g_vfs->readlink(path);
        const auto len = std::min(sz, link_path.size());
        std::copy(link_path.c_str(), link_path.c_str() + len, dst);
        dst[len] = '\0';
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int mkdir(const char* path, mode_t mode) noexcept
{
    log_trace("path:{}", path);
    try
    {
        g_vfs->mkdir(path, mode | S_IFDIR);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int rmdir(const char* path) noexcept
{
    log_trace("path:{}", path);
    try
    {
        g_vfs->rmdir(path);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int unlink(const char* path) noexcept
{
    log_trace("path:{}", path);
    try
    {
        g_vfs->unlink(path);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int symlink(const char* target, const char* link_path) noexcept
{
    log_trace("path:{}", link_path);
    try
    {
        g_vfs->symlink(target, link_path);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int rename(const char* old_path, const char* new_path, unsigned int flags) noexcept
{
    log_trace("path:{}->{}", old_path, new_path);
    try
    {
        g_vfs->rename(old_path, new_path, flags);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int chmod(const char* path, const mode_t mode, struct fuse_file_info*) noexcept
{
    log_trace("path:{}", path);
    try
    {
        g_vfs->chmod(path, mode);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int truncate(const char* path, const off_t length, struct fuse_file_info*) noexcept
{
    log_trace("path:{}", path);
    try
    {
        if (length < 0)
        {
            throw std::system_error{EINVAL, std::generic_category()};
        }
        g_vfs->truncate(path, length);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int create(const char* path, mode_t mode, struct fuse_file_info* fi) noexcept
{
    log_trace("path:{} mode:{:o}", path, mode);
    try
    {
        fi->fh = g_vfs->create(path, fi->flags, mode).value_of();
        log_trace("handle:{}", fi->fh);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int open(const char* path, struct fuse_file_info *fi) noexcept
{
    log_trace("path:{}", path);
    try
    {
        fi->fh = g_vfs->open(path, fi->flags).value_of();
        log_trace("handle:{}", fi->fh);
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int release(const char* path, struct fuse_file_info *fi) noexcept
{
    log_trace("path:{} handle:{}", path, fi->fh);
    try
    {
        g_vfs->close(IVfs::FileHandle{fi->fh});
    }
    catch (...)
    {
        return gen_return_error_code();
    }
    return 0;
}

static int read(const char* path, char* output, size_t size, off_t offset,
                struct fuse_file_info* fi) noexcept
{
    log_trace("path:{} handle:{} size:{} ofs:{}", path, fi->fh, size, offset);
    try
    {
        return static_cast<int>(g_vfs->read(IVfs::FileHandle{fi->fh},
                                            {reinterpret_cast<uint8_t*>(output), size},
                                            offset));
    }
    catch (...)
    {
        return gen_return_error_code();
    }
}

static int write(const char* path, const char* input, size_t size, off_t offset,
                struct fuse_file_info* fi) noexcept
{
    log_trace("path:{} handle:{} size:{} ofs:{}", path, fi->fh, size, offset);
    try
    {
        return static_cast<int>(
            g_vfs->write(IVfs::FileHandle{fi->fh},
                         {reinterpret_cast<const uint8_t*>(input), size}, offset));
    }
    catch (...)
    {
        return gen_return_error_code();
    }
}

//==========================================================================
} // namespace callbacks
//==========================================================================

Fuse::Fuse(IVfs& vfs)
    : m_vfs{vfs}
{
    g_vfs = &m_vfs;

    g_oper.getattr = callbacks::getattr;
    g_oper.readdir = callbacks::readdir;
    g_oper.readlink = callbacks::readlink;
    g_oper.mkdir = callbacks::mkdir;
    g_oper.rmdir = callbacks::rmdir;
    g_oper.unlink = callbacks::unlink;
    g_oper.symlink = callbacks::symlink;
    g_oper.rename = callbacks::rename;
    g_oper.chmod = callbacks::chmod;
    g_oper.truncate = callbacks::truncate;
    g_oper.create = callbacks::create;
    g_oper.open = callbacks::open;
    g_oper.release = callbacks::release;
    g_oper.read = callbacks::read;
    g_oper.write = callbacks::write;
}

//--------------------------------------------------------------------------

void Fuse::set_mountpoint(const std::string& path)
{
    m_mountpoint = path;
}

//--------------------------------------------------------------------------

void Fuse::start()
{
    m_thread = std::thread(&Fuse::run, this);
}

//--------------------------------------------------------------------------

void Fuse::wait()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

//--------------------------------------------------------------------------

void Fuse::run()
{
    log_info("starting FUSE");

    struct fuse_args args{};
    args.argc = 1;
    std::string fake_cmd_name{"rewofs"};
    char* argv[] = {fake_cmd_name.data(), nullptr};
    args.argc = 1;
    args.argv = argv;
    args.allocated = 0;
    auto fuse = fuse_new(&args, &g_oper, sizeof(g_oper), nullptr);
    if (fuse == nullptr)
    {
        throw std::runtime_error{"can't init"};
    }

    log_info("mountpoint: '{}'", m_mountpoint);
    const auto mount_res = fuse_mount(fuse, m_mountpoint.c_str());
    if (mount_res == -1)
    {
        log_error("errno: {}", errno);
        throw std::runtime_error{"can't mount"};
    }

    const auto signal_res = fuse_set_signal_handlers(fuse_get_session(fuse));
    if (signal_res == -1)
    {
        throw std::runtime_error{"can't set signal handlers"};
    }

    log_info("looping");
    fuse_loop_config loop_config{};
    loop_config.clone_fd = 0;
    loop_config.max_idle_threads = 10;
    fuse_loop_mt(fuse, &loop_config);
    log_info("quitting");
    fuse_unmount(fuse);
}

//==========================================================================
} // namespace rewofs::client
