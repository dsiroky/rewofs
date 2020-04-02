/// Virtual File System.
///
/// @file

#pragma once
#ifndef VFS_HPP__TI3ABKYJ
#define VFS_HPP__TI3ABKYJ

#include <mutex>
#include <optional>
#include <unordered_map>

#include <sys/stat.h>

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/client/config.hpp"
#include "rewofs/client/cache.hpp"
#include "rewofs/client/transport.hpp"
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
    virtual Path readlink(const Path& path) = 0;
    virtual void mkdir(const Path& path, mode_t mode) = 0;
    virtual void rmdir(const Path& path) = 0;
    virtual void unlink(const Path& path) = 0;
    virtual void symlink(const Path& target, const Path& link_path) = 0;
    virtual void rename(const Path& old_path, const Path& new_path)
        = 0;
    virtual void chmod(const Path& path, const mode_t mode) = 0;
    virtual void truncate(const Path& path, const off_t length) = 0;
    virtual FileHandle open(const Path& path, const int flags, const mode_t mode) = 0;
    virtual FileHandle open(const Path& path, const int flags) = 0;
    virtual void close(const FileHandle fh) = 0;
    virtual size_t read(const FileHandle fh, const gsl::span<uint8_t> output,
                        const off_t offset)
        = 0;
    virtual size_t write(const FileHandle fh, const gsl::span<const uint8_t> input,
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

    void getattr(const Path& path, struct stat& st) override;
    void readdir(const Path& path, const DirFiller& filler) override;
    Path readlink(const Path& path) override;
    void mkdir(const Path& path, mode_t mode) override;
    void rmdir(const Path& path) override;
    void unlink(const Path& path) override;
    void symlink(const Path& target, const Path& link_path) override;
    void rename(const Path& old_path, const Path& new_path) override;
    void chmod(const Path& path, const mode_t mode) override;
    void truncate(const Path& path, const off_t length) override;
    FileHandle open(const Path& path, const int flags, const mode_t mode) override;
    FileHandle open(const Path& path, const int flags) override;
    void close(const FileHandle fh) override;
    size_t read(const FileHandle fh, const gsl::span<uint8_t> output,
                const off_t offset) override;
    size_t write(const FileHandle fh, const gsl::span<const uint8_t> input,
                 const off_t offset) override;

    //--------------------------------
private:
    // fragment reads/writes to improve remote response
    static constexpr size_t IO_FRAGMENT_SIZE{1024 * 32};

    struct FileParams
    {
        Path path{};
    };

    FileHandle open_common(const Path& path, const int flags,
                           const std::optional<mode_t> mode);

    Serializer& m_serializer;
    Deserializer& m_deserializer;
    SingleComm m_comm{m_serializer, m_deserializer};
    std::atomic<uint64_t> m_open_id_dispenser{};
    std::mutex m_mutex{};
    std::unordered_map<FileHandle, FileParams> m_opened_files{};
};

//==========================================================================

class CachedVfs:public IVfs
{
public:
    CachedVfs(IVfs& subvfs, Serializer& serializer, Deserializer& deserializer);

    /// Prefill the tree.
    void populate_tree();

    void getattr(const Path&, struct stat& st) override;
    void readdir(const Path&, const DirFiller& filler) override;
    Path readlink(const Path& path) override;
    void mkdir(const Path& path, mode_t mode) override;
    void rmdir(const Path& path) override;
    void unlink(const Path& path) override;
    void symlink(const Path& target, const Path& link_path) override;
    void rename(const Path& old_path, const Path& new_path) override;
    void chmod(const Path& path, const mode_t mode) override;
    void truncate(const Path& path, const off_t length) override;
    FileHandle open(const Path& path, const int flags, const mode_t mode) override;
    FileHandle open(const Path& path, const int flags) override;
    void close(const FileHandle fh) override;
    size_t read(const FileHandle fh, const gsl::span<uint8_t> output,
                const off_t offset) override;
    size_t write(const FileHandle fh, const gsl::span<const uint8_t> input,
                 const off_t offset) override;

private:
    void populate_tree(cache::Node& node, const messages::TreeNode& fbb_node);

    IVfs& m_subvfs;
    Serializer& m_serializer;
    Deserializer& m_deserializer;
    SingleComm m_comm{m_serializer, m_deserializer};
    cache::Tree m_tree{};
    std::mutex m_tree_mutex{};
};

//==========================================================================
} // namespace rewofs::client::vfs

#endif /* include guard */
