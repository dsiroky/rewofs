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

class IdDispenser
{
public:
    /// Seed for unique identifiers.
    void set_seed(const uint64_t seed);

    uint64_t get();

private:
    std::atomic<uint64_t> m_dispenser{};
};

//==========================================================================

class IVfs
{
public:
    // fragment reads/writes to improve remote response
    static constexpr size_t IO_FRAGMENT_SIZE{32 * 1024};

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
    virtual void rename(const Path& old_path, const Path& new_path, const uint32_t flags)
        = 0;
    virtual void chmod(const Path& path, const mode_t mode) = 0;
    virtual void utimens(const Path& path, const struct timespec tv[2]) = 0;
    virtual void truncate(const Path& path, const off_t length) = 0;
    virtual FileHandle create(const Path& path, const int flags, const mode_t mode) = 0;
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
    RemoteVfs(Serializer& serializer, Deserializer& deserializer,
              IdDispenser& id_dispenser);

    void getattr(const Path& path, struct stat& st) override;
    void readdir(const Path& path, const DirFiller& filler) override;
    Path readlink(const Path& path) override;
    void mkdir(const Path& path, mode_t mode) override;
    void rmdir(const Path& path) override;
    void unlink(const Path& path) override;
    void symlink(const Path& target, const Path& link_path) override;
    void rename(const Path& old_path, const Path& new_path, const uint32_t flags) override;
    void chmod(const Path& path, const mode_t mode) override;
    void utimens(const Path& path, const struct timespec tv[2]) override;
    void truncate(const Path& path, const off_t length) override;
    FileHandle create(const Path& path, const int flags, const mode_t mode) override;
    FileHandle open(const Path& path, const int flags) override;
    void close(const FileHandle fh) override;
    size_t read(const FileHandle fh, const gsl::span<uint8_t> output,
                const off_t offset) override;
    size_t write(const FileHandle fh, const gsl::span<const uint8_t> input,
                 const off_t offset) override;

    //--------------------------------
private:
    FileHandle open_common(const Path& path, const int flags,
                           const std::optional<mode_t> mode);

    Serializer& m_serializer;
    Deserializer& m_deserializer;
    IdDispenser& m_id_dispenser;
    SingleComm m_comm{m_serializer, m_deserializer};
};

//==========================================================================

class CachedVfs:public IVfs
{
public:
    CachedVfs(IVfs& subvfs, Serializer& serializer, Deserializer& deserializer,
              IdDispenser& id_dispenser, cache::Cache& cache);

    void getattr(const Path&, struct stat& st) override;
    void readdir(const Path&, const DirFiller& filler) override;
    Path readlink(const Path& path) override;
    void mkdir(const Path& path, mode_t mode) override;
    void rmdir(const Path& path) override;
    void unlink(const Path& path) override;
    void symlink(const Path& target, const Path& link_path) override;
    void rename(const Path& old_path, const Path& new_path, const uint32_t flags) override;
    void chmod(const Path& path, const mode_t mode) override;
    void utimens(const Path& path, const struct timespec tv[2]) override;
    void truncate(const Path& path, const off_t length) override;
    FileHandle create(const Path& path, const int flags, const mode_t mode) override;
    FileHandle open(const Path& path, const int flags) override;
    void close(const FileHandle fh) override;
    size_t read(const FileHandle fh, const gsl::span<uint8_t> output,
                const off_t offset) override;
    size_t write(const FileHandle fh, const gsl::span<const uint8_t> input,
                 const off_t offset) override;

private:
    struct File
    {
        int open_flags{};
        std::optional<FileHandle> subvfs_handle{};
        Path path{};
    };

    IVfs& m_subvfs;
    Serializer& m_serializer;
    Deserializer& m_deserializer;
    IdDispenser& m_id_dispenser;
    SingleComm m_comm{m_serializer, m_deserializer};
    cache::Cache& m_cache;
    std::unordered_map<FileHandle, File> m_opened_files{};
};

//==========================================================================

/// Tree and content (pre)loader.
class BackgroundLoader
{
public:
    BackgroundLoader(Serializer& serializer, Deserializer& deserializer,
                     Distributor& distributor, cache::Cache& cache);

    void start();
    void stop();
    void wait();

    void invalidate_tree();

private:
    struct FileInfo
    {
        IVfs::Path path{};
        uint64_t size{};
    };

    /// Prefill the tree.
    void populate_tree();
    void populate_tree(cache::Node& node, const messages::TreeNode& fbb_node);
    template<typename _It>
    void preload_files_bulks(const _It begin, const _It end);
    void preload_files();
    void tree_loader();
    void process_remote_changed(const messages::NotifyChanged&);

    Serializer& m_serializer;
    Deserializer& m_deserializer;
    Distributor& m_distributor;
    SingleComm m_comm{m_serializer, m_deserializer};
    cache::Cache& m_cache;
    std::thread m_tree_loader_thread{};
    std::condition_variable m_cv{};
    std::atomic<bool> m_tree_invalidated{false};
    std::atomic<bool> m_quit{false};
};

//==========================================================================
} // namespace rewofs::client::vfs

#endif /* include guard */
