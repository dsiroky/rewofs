/// Virtual File System.
///
/// @file

#pragma once
#ifndef VFS_HPP__TI3ABKYJ
#define VFS_HPP__TI3ABKYJ

#include <sys/stat.h>

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/transport.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

class IVfs
{
public:
    virtual ~IVfs() = default;

    using Path = boost::filesystem::path;
    using DirFiller = std::function<void(const Path&, const struct stat&)>;

    virtual void getattr(const Path& path, struct stat& st) = 0;
    virtual void readdir(const Path& path, const DirFiller& filler)
        = 0;
    virtual std::string readlink(const Path& path) = 0;
    virtual void mkdir(const Path& path, mode_t mode) = 0;

private:
};

//==========================================================================

class RemoteVfs : public IVfs, private boost::noncopyable
{
public:
    RemoteVfs(Serializer& serializer, Deserializer& deserializer);

    void getattr(const Path&, struct stat& st) override;
    void readdir(const Path&, const DirFiller& filler) override;
    std::string readlink(const Path& path) override;
    void mkdir(const Path& path, mode_t mode) override;

    //--------------------------------
private:
    template<typename _Result, typename _Command>
    Deserializer::Result<_Result>
        single_command(flatbuffers::FlatBufferBuilder& fbb,
                       const flatbuffers::Offset<_Command> command);

    Serializer& m_serializer;
    Deserializer& m_deserializer;
    static constexpr std::chrono::seconds TIMEOUT{5};
};

//--------------------------------------------------------------------------

template<typename _Result, typename _Command>
Deserializer::Result<_Result>
    RemoteVfs::single_command(flatbuffers::FlatBufferBuilder& fbb,
                              const flatbuffers::Offset<_Command> command)
{
    auto queue = m_serializer.new_queue(Serializer::PRIORITY_DEFAULT);
    const auto mid = m_serializer.add_command(queue, fbb, command);
    log_trace("mid:{}", strong::value_of(mid));
    const auto res = m_deserializer.wait_for_result<_Result>(mid, TIMEOUT);
    if (not res.is_valid())
    {
        throw std::system_error{EHOSTUNREACH, std::generic_category()};
    }
    return res;
}

//==========================================================================
} // namespace rewofs::client::vfs

#endif /* include guard */
