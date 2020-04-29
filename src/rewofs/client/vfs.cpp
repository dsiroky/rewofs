/// @copydoc vfs.hpp
///
/// @file

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "rewofs/client/vfs.hpp"
#include "rewofs/transport.hpp"

//==========================================================================
namespace rewofs::client {
//==========================================================================

//==========================================================================
namespace {
//==========================================================================

void copy(const messages::Time& src, timespec& dst)
{
    dst.tv_nsec = src.nsec();
    dst.tv_sec = src.sec();
}

//--------------------------------------------------------------------------

void copy(const messages::Stat& src, struct stat& dst)
{
    dst.st_mode = src.st_mode();
    dst.st_size = src.st_size();
    copy(src.st_ctim(), dst.st_ctim);
    copy(src.st_mtim(), dst.st_mtim);
}

//==========================================================================
} // namespace
//==========================================================================

void IdDispenser::set_seed(const uint64_t seed)
{
    m_dispenser = seed;
}

//--------------------------------------------------------------------------

uint64_t IdDispenser::get()
{
    return m_dispenser++;
}

//==========================================================================

RemoteVfs::RemoteVfs(Serializer& serializer, Deserializer& deserializer,
                     IdDispenser& id_dispenser)
    : m_serializer{serializer}
    , m_deserializer{deserializer}
    , m_id_dispenser{id_dispenser}
{
}

//--------------------------------------------------------------------------

void RemoteVfs::getattr(const Path& path, struct stat& st)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandStatDirect(fbb, path.c_str());
    const auto res = m_comm.single_command<messages::ResultStat>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
    assert(message.st() != nullptr);
    copy(*message.st(), st);
}

//--------------------------------------------------------------------------

void RemoteVfs::readdir(const Path& path, const DirFiller& filler)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandReaddirDirect(fbb, path.c_str());
    const auto res = m_comm.single_command<messages::ResultReaddir>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    for (const auto& item: *message.items())
    {
        if (item->st() == nullptr)
        {
            filler(item->name()->str(), {});
        }
        else
        {
            struct stat st{};
            copy(*item->st(), st);
            filler(item->name()->str(), st);
        }
    }
}

//--------------------------------------------------------------------------

IVfs::Path RemoteVfs::readlink(const Path& path)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandReadlinkDirect(fbb, path.c_str());
    const auto res = m_comm.single_command<messages::ResultReadlink>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    return message.path()->c_str();
}

//--------------------------------------------------------------------------

void RemoteVfs::mkdir(const Path& path, mode_t mode)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandMkdirDirect(fbb, path.c_str(), mode);
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::rmdir(const Path& path)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandRmdirDirect(fbb, path.c_str());
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::unlink(const Path& path)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandUnlinkDirect(fbb, path.c_str());
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::symlink(const Path& target, const Path& link_path)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command
        = messages::CreateCommandSymlinkDirect(fbb, link_path.c_str(), target.c_str());
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::rename(const Path& old_path, const Path& new_path, const uint32_t flags)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandRenameDirect(fbb, old_path.c_str(),
                                                             new_path.c_str(), flags);
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::chmod(const Path& path, const mode_t mode)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandChmodDirect(fbb, path.c_str(), mode);
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::truncate(const Path& path, const off_t length)
{
    if (length < 0)
    {
        throw std::system_error{EINVAL, std::generic_category()};
    }

    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandTruncateDirect(
        fbb, path.c_str(), static_cast<uint64_t>(length));
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

IVfs::FileHandle RemoteVfs::open_common(const Path& path, const int flags,
                                        const std::optional<mode_t> mode)
{
    log_trace("opening '{}'", path.native());
    flatbuffers::FlatBufferBuilder fbb{};
    const auto new_open_id = m_id_dispenser.get();
    const auto msg_path = fbb.CreateString(path.native());
    messages::CommandOpenBuilder cmd_bld{fbb};
    cmd_bld.add_path(msg_path);
    cmd_bld.add_file_handle(new_open_id);
    cmd_bld.add_flags(flags);
    if (mode.has_value())
    {
        cmd_bld.add_mode(*mode);
    }
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, cmd_bld.Finish());

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    log_trace("open fh:{} '{}'", new_open_id, path.native());
    return FileHandle{new_open_id};
}

//--------------------------------------------------------------------------

IVfs::FileHandle RemoteVfs::create(const Path& path, const int flags, const mode_t mode)
{
    return open_common(path, flags, mode);
}

//--------------------------------------------------------------------------

IVfs::FileHandle RemoteVfs::open(const Path& path, const int flags)
{
    return open_common(path, flags, std::nullopt);
}

//--------------------------------------------------------------------------

void RemoteVfs::close(const FileHandle fh)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandClose(fbb, strong::value_of(fh));
    const auto res = m_comm.single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

size_t RemoteVfs::read(const FileHandle fh, const gsl::span<uint8_t> output,
                       const off_t offset)
{
    auto queue = m_serializer.new_queue(Serializer::PRIORITY_DEFAULT);
    std::vector<MessageId> mids{};

    // queue chunks to improve responses over slow lines
    size_t block_ofs{0};
    mids.reserve(output.size() / IO_FRAGMENT_SIZE + 1);
    while (block_ofs < output.size())
    {
        const size_t block_size{std::min(output.size() - block_ofs, IO_FRAGMENT_SIZE)};

        flatbuffers::FlatBufferBuilder fbb{};
        const auto command = messages::CreateCommandRead(
            fbb, strong::value_of(fh), static_cast<size_t>(offset) + block_ofs,
            block_size);
        mids.emplace_back(m_serializer.add_command(queue, fbb, command));
        log_trace("mid:{}", strong::value_of(mids.back()));
        block_ofs += block_size;
    }

    size_t read_size{0};
    for (const auto mid: mids)
    {
        const auto res
            = m_deserializer.wait_for_result<messages::ResultRead>(mid, TIMEOUT);
        if (not res.is_valid())
        {
            throw std::system_error{EHOSTUNREACH, std::generic_category()};
        }
        const auto& message = res.message();
        if (message.res() < 0)
        {
            throw std::system_error{message.res_errno(), std::generic_category()};
        }

        std::copy(message.data()->begin(), message.data()->end(),
                  output.begin() + static_cast<ssize_t>(read_size));
        read_size += message.data()->size();
    }

    return read_size;
}

//--------------------------------------------------------------------------

size_t RemoteVfs::write(const FileHandle fh, const gsl::span<const uint8_t> input,
                        const off_t offset)
{
    auto queue = m_serializer.new_queue(Serializer::PRIORITY_DEFAULT);
    std::vector<MessageId> mids{};

    // queue chunks to improve responses over slow lines
    size_t block_ofs{0};
    mids.reserve(input.size() / IO_FRAGMENT_SIZE + 1);
    while (block_ofs < input.size())
    {
        const size_t block_size{std::min(input.size() - block_ofs, IO_FRAGMENT_SIZE)};

        flatbuffers::FlatBufferBuilder fbb{};
        const auto data = fbb.CreateVector(input.data() + block_ofs, block_size);
        const auto command = messages::CreateCommandWrite(
            fbb, strong::value_of(fh), static_cast<size_t>(offset) + block_ofs, data);
        mids.emplace_back(m_serializer.add_command(queue, fbb, command));
        log_trace("mid:{}", strong::value_of(mids.back()));
        block_ofs += block_size;
    }

    size_t write_size{0};
    for (const auto mid: mids)
    {
        const auto res
            = m_deserializer.wait_for_result<messages::ResultWrite>(mid, TIMEOUT);
        if (not res.is_valid())
        {
            throw std::system_error{EHOSTUNREACH, std::generic_category()};
        }
        const auto& message = res.message();
        if (message.res() < 0)
        {
            throw std::system_error{message.res_errno(), std::generic_category()};
        }

        write_size += static_cast<size_t>(message.res());
    }

    return write_size;
}

//==========================================================================

CachedVfs::CachedVfs(IVfs& subvfs, Serializer& serializer, Deserializer& deserializer,
                     IdDispenser& id_dispenser)
    : m_subvfs{subvfs}
    , m_serializer{serializer}
    , m_deserializer{deserializer}
    , m_id_dispenser{id_dispenser}
{
}

//--------------------------------------------------------------------------

void CachedVfs::populate_tree()
{
    log_info("populating tree");

    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandReadTreeDirect(fbb, "/");
    const auto res = m_comm.single_command<messages::ResultReadTree>(
        fbb, command, std::chrono::seconds{60});

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    std::lock_guard lg{m_mutex};
    m_content_cache.reset();
    m_tree.reset();
    populate_tree(m_tree.get_root(), *message.tree());

    log_info("populating tree done");
}

//--------------------------------------------------------------------------

void CachedVfs::populate_tree(cache::Node& node, const messages::TreeNode& fbb_node)
{
    node.name = fbb_node.name()->str();
    copy(*fbb_node.st(), node.st);
    for (const auto& child: *fbb_node.children())
    {
        auto& new_child = m_tree.make_node(node, child->name()->str());
        populate_tree(new_child, *child);
    }
}

//--------------------------------------------------------------------------

void CachedVfs::getattr(const Path& path, struct stat& st)
{
    std::lock_guard lg{m_mutex};
    st = m_tree.get_node(path).st;
}

//--------------------------------------------------------------------------

void CachedVfs::readdir(const Path& path, const DirFiller& filler)
{
    std::lock_guard lg{m_mutex};
    const auto& node = m_tree.get_node(path);

    for (const auto& [name, child]: node.children)
    {
        filler(name, child.st);
    }
}

//--------------------------------------------------------------------------

IVfs::Path CachedVfs::readlink(const Path& path)
{
    return m_subvfs.readlink(path);
}

//--------------------------------------------------------------------------

void CachedVfs::mkdir(const Path& path, mode_t mode)
{
    m_subvfs.mkdir(path, mode);

    struct stat st{};
    m_subvfs.getattr(path, st);
    struct stat parent_st{};
    m_subvfs.getattr(path.parent_path(), parent_st);

    std::lock_guard lg{m_mutex};
    m_tree.get_node(path.parent_path()).st = parent_st;
    auto& new_node = m_tree.make_node(path);
    new_node.st = st;
}

//--------------------------------------------------------------------------

void CachedVfs::rmdir(const Path& path)
{
    m_subvfs.rmdir(path);

    struct stat parent_st{};
    m_subvfs.getattr(path.parent_path(), parent_st);

    std::lock_guard lg{m_mutex};
    m_tree.remove_single(path);
    m_tree.get_node(path.parent_path()).st = parent_st;
}

//--------------------------------------------------------------------------

void CachedVfs::unlink(const Path& path)
{
    m_subvfs.unlink(path);

    struct stat parent_st{};
    m_subvfs.getattr(path.parent_path(), parent_st);

    std::lock_guard lg{m_mutex};
    m_tree.remove_single(path);
    m_content_cache.delete_file(path);
    m_tree.get_node(path.parent_path()).st = parent_st;
}

//--------------------------------------------------------------------------

void CachedVfs::symlink(const Path& target, const Path& link_path)
{
    m_subvfs.symlink(target, link_path);

    struct stat st{};
    m_subvfs.getattr(link_path, st);
    struct stat parent_st{};
    m_subvfs.getattr(link_path.parent_path(), parent_st);

    std::lock_guard lg{m_mutex};
    m_tree.make_node(link_path).st = st;
    m_tree.get_node(link_path.parent_path()).st = parent_st;
}

//--------------------------------------------------------------------------

void CachedVfs::rename(const Path& old_path, const Path& new_path, const uint32_t flags)
{
    m_subvfs.rename(old_path, new_path, flags);

    std::lock_guard lg{m_mutex};
#ifndef RENAME_EXCHANGE
    #define RENAME_EXCHANGE (1 << 1)
#endif
    if (flags & RENAME_EXCHANGE)
    {
        m_tree.exchange(old_path, new_path);
    }
    else
    {
        m_tree.rename(old_path, new_path);
    }
    // TODO m_content_cache
}

//--------------------------------------------------------------------------

void CachedVfs::chmod(const Path& path, const mode_t mode)
{
    m_subvfs.chmod(path, mode);
    std::lock_guard lg{m_mutex};
    m_tree.get_node(path).st.st_mode = mode;
}

//--------------------------------------------------------------------------

void CachedVfs::truncate(const Path& path, const off_t length)
{
    m_subvfs.truncate(path, length);

    // TODO let the create command return the stat
    struct stat st{};
    m_subvfs.getattr(path, st);

    std::lock_guard lg{m_mutex};
    auto& node = m_tree.get_node(path);
    node.st = st;

    // TODO m_content_cache
}

//--------------------------------------------------------------------------

IVfs::FileHandle CachedVfs::create(const Path& path, const int flags, const mode_t mode)
{
    const auto subvfs_handle = m_subvfs.create(path, flags, mode);

    // TODO let the create command return the stat
    struct stat st{};
    m_subvfs.getattr(path, st);

    std::lock_guard lg{m_mutex};
    m_tree.make_node(path).st = st;
    File file{flags, subvfs_handle, path};
    const auto handle = FileHandle{m_id_dispenser.get()};
    m_opened_files.emplace(std::make_pair(handle, std::move(file)));
    return handle;
}

//--------------------------------------------------------------------------

IVfs::FileHandle CachedVfs::open(const Path& path, const int flags)
{
    std::optional<FileHandle> subvfs_handle{};
    // lazy open on read only
    if ((flags & O_WRONLY) or (flags & O_RDWR) or (flags & O_APPEND))
    {
        subvfs_handle = m_subvfs.open(path, flags);
    }
    File file{flags, subvfs_handle, path};
    std::lock_guard lg{m_mutex};
    const auto handle = FileHandle{m_id_dispenser.get()};
    m_opened_files.emplace(std::make_pair(handle, std::move(file)));
    return handle;
}

//--------------------------------------------------------------------------

void CachedVfs::close(const FileHandle fh)
{
    std::unique_lock lg{m_mutex};
    const auto it = m_opened_files.find(fh);
    if (it == m_opened_files.end())
    {
        throw std::system_error{EBADF, std::generic_category()};
    }
    const auto subhandle = it->second.subvfs_handle;
    lg.unlock();

    if (subhandle.has_value())
    {
        m_subvfs.close(*subhandle);
    }

    lg.lock();
    m_opened_files.erase(it);
}

//--------------------------------------------------------------------------

size_t CachedVfs::read(const FileHandle fh, const gsl::span<uint8_t> output,
                       const off_t offset)
{
    std::unique_lock lg{m_mutex};
    const auto it = m_opened_files.find(fh);
    if (it == m_opened_files.end())
    {
        throw std::system_error{EBADF, std::generic_category()};
    }

    bool has_cached_block = m_content_cache.read(
        it->second.path, static_cast<uintmax_t>(offset), output.size(),
        [&output](const auto& content) {
            assert(content.size() == output.size());
            std::copy(content.begin(), content.end(), output.begin());
        });

    if (has_cached_block)
    {
        log_trace("cache hit");
        return output.size();
    }
    else
    {
        log_trace("cache miss");
        auto subhandle = it->second.subvfs_handle;
        lg.unlock();
        if (not subhandle.has_value())
        {
            subhandle = m_subvfs.open(it->second.path, it->second.open_flags);
        }
        const auto ret = m_subvfs.read(*subhandle, output, offset);
        lg.lock();
        const auto refreshed_it = m_opened_files.find(fh);
        refreshed_it->second.subvfs_handle = subhandle;
        m_content_cache.write(refreshed_it->second.path, static_cast<uintmax_t>(offset),
                              {output.begin(), output.end()});
        return ret;
    }
}

//--------------------------------------------------------------------------

size_t CachedVfs::write(const FileHandle fh, const gsl::span<const uint8_t> input,
                        const off_t offset)
{
    std::unique_lock lg{m_mutex};
    const auto it = m_opened_files.find(fh);
    if ((it == m_opened_files.end()) or (not it->second.subvfs_handle.has_value()))
    {
        throw std::system_error{EBADF, std::generic_category()};
    }
    const auto file = it->second;
    lg.unlock();

    const auto res = m_subvfs.write(*file.subvfs_handle, input, offset);

    lg.lock();

    // TODO let the main command return the stat
    struct stat st{};
    m_subvfs.getattr(file.path, st);

    auto& node = m_tree.get_node(file.path);
    node.st = st;
    m_content_cache.write(file.path, static_cast<uintmax_t>(offset),
                          {input.begin(), input.end()});

    return res;
}

//==========================================================================
} // namespace rewofs::client
