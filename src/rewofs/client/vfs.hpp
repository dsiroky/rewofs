/// Virtual File System.
///
/// @file

#pragma once
#ifndef VFS_HPP__TI3ABKYJ
#define VFS_HPP__TI3ABKYJ

#include <optional>
#include <unordered_map>

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
    using FileHandle
        = strong::type<uint64_t, struct FileHandle_, strong::equality, strong::hashable>;

    virtual ~IVfs() = default;

    using Path = boost::filesystem::path;
    using DirFiller = std::function<void(const Path&, const struct stat&)>;

    virtual void getattr(const Path& path, struct stat& st) = 0;
    virtual void readdir(const Path& path, const DirFiller& filler)
        = 0;
    virtual std::string readlink(const Path& path) = 0;
    virtual void mkdir(const Path& path, mode_t mode) = 0;
    virtual void rmdir(const Path& path) = 0;
    virtual void unlink(const Path& path) = 0;
    virtual void symlink(const Path& target, const Path& link_path) = 0;
    virtual void rename(const Path& old_path, const Path& new_path)
        = 0;
    virtual void chmod(const Path& path, const mode_t mode) = 0;
    virtual uint64_t open(const Path& path, const int flags, const mode_t mode) = 0;
    virtual uint64_t open(const Path& path, const int flags) = 0;
    virtual ssize_t read(const FileHandle fh, const gsl::span<uint8_t> output,
                         const off_t offset)
        = 0;

private:
};

//==========================================================================

class RemoteVfs : public IVfs, private boost::noncopyable
{
public:
    RemoteVfs(Serializer& serializer, Deserializer& deserializer);

    /// Seed for unique identifiers.
    void set_seed(const uint64_t seed);

    void getattr(const Path&, struct stat& st) override;
    void readdir(const Path&, const DirFiller& filler) override;
    std::string readlink(const Path& path) override;
    void mkdir(const Path& path, mode_t mode) override;
    void rmdir(const Path& path) override;
    void unlink(const Path& path) override;
    void symlink(const Path& target, const Path& link_path) override;
    void rename(const Path& old_path, const Path& new_path) override;
    void chmod(const Path& path, const mode_t mode) override;
    uint64_t open(const Path& path, const int flags, const mode_t mode) override;
    uint64_t open(const Path& path, const int flags) override;
    ssize_t read(const FileHandle fh, const gsl::span<uint8_t> output,
                 const off_t offset) override;

    //--------------------------------
private:
    static constexpr std::chrono::seconds TIMEOUT{5};

    struct FileParams
    {
        Path path{};
        struct
        {
            int32_t flags{};
            std::optional<uint32_t> mode{};
        } open_params{};
    };

    uint64_t open_common(const Path& path, const int flags,
                         const std::optional<mode_t> mode);

    template<typename _Result, typename _Command>
    Deserializer::Result<_Result>
        single_command(flatbuffers::FlatBufferBuilder& fbb,
                       const flatbuffers::Offset<_Command> command);

    Serializer& m_serializer;
    Deserializer& m_deserializer;
    std::atomic<uint64_t> m_open_id_dispenser{};
    std::mutex m_mutex{};
    std::unordered_map<FileHandle, FileParams> m_opened_files{};
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
