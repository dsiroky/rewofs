/// @copydoc vfs.hpp
///
/// @file

#include <regex>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "rewofs/client/vfs.hpp"
#include "rewofs/messages.hpp"
#include "rewofs/transport.hpp"

//==========================================================================
namespace rewofs::client {
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

void RemoteVfs::utimens(const Path& path, const struct timespec tv[2])
{
    flatbuffers::FlatBufferBuilder fbb{};
    // work only with mtime
    messages::Time mtime{};
    copy(tv[1], mtime);
    const auto command = messages::CreateCommandUtimeDirect(fbb, path.c_str(), &mtime);
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
                     IdDispenser& id_dispenser, cache::Cache& cache)
    : m_subvfs{subvfs}
    , m_serializer{serializer}
    , m_deserializer{deserializer}
    , m_id_dispenser{id_dispenser}
    , m_cache{cache}
{
}

//--------------------------------------------------------------------------

void CachedVfs::getattr(const Path& path, struct stat& st)
{
    auto lg = m_cache.lock();
    st = m_cache.get_node(path).st;
}

//--------------------------------------------------------------------------

void CachedVfs::readdir(const Path& path, const DirFiller& filler)
{
    auto lg = m_cache.lock();
    const auto& node = m_cache.get_node(path);

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

    auto lg = m_cache.lock();
    m_cache.get_node(path.parent_path()).st = parent_st;
    auto& new_node = m_cache.make_node(path);
    new_node.st = st;
}

//--------------------------------------------------------------------------

void CachedVfs::rmdir(const Path& path)
{
    m_subvfs.rmdir(path);

    struct stat parent_st{};
    m_subvfs.getattr(path.parent_path(), parent_st);

    auto lg = m_cache.lock();
    m_cache.remove_single(path);
    m_cache.get_node(path.parent_path()).st = parent_st;
}

//--------------------------------------------------------------------------

void CachedVfs::unlink(const Path& path)
{
    m_subvfs.unlink(path);

    struct stat parent_st{};
    m_subvfs.getattr(path.parent_path(), parent_st);

    auto lg = m_cache.lock();
    m_cache.remove_single(path);
    m_cache.get_node(path.parent_path()).st = parent_st;
}

//--------------------------------------------------------------------------

void CachedVfs::symlink(const Path& target, const Path& link_path)
{
    m_subvfs.symlink(target, link_path);

    struct stat st{};
    m_subvfs.getattr(link_path, st);
    struct stat parent_st{};
    m_subvfs.getattr(link_path.parent_path(), parent_st);

    auto lg = m_cache.lock();
    m_cache.make_node(link_path).st = st;
    m_cache.get_node(link_path.parent_path()).st = parent_st;
}

//--------------------------------------------------------------------------

void CachedVfs::rename(const Path& old_path, const Path& new_path, const uint32_t flags)
{
    m_subvfs.rename(old_path, new_path, flags);

    auto lg = m_cache.lock();
#ifndef RENAME_EXCHANGE
    #define RENAME_EXCHANGE (1 << 1)
#endif
    if (flags & RENAME_EXCHANGE)
    {
        m_cache.exchange(old_path, new_path);
    }
    else
    {
        m_cache.rename(old_path, new_path);
    }
}

//--------------------------------------------------------------------------

void CachedVfs::chmod(const Path& path, const mode_t mode)
{
    m_subvfs.chmod(path, mode);
    auto lg = m_cache.lock();
    m_cache.get_node(path).st.st_mode = mode;
}

//--------------------------------------------------------------------------

void CachedVfs::utimens(const Path& path, const struct timespec tv[2])
{
    // work only with mtime

    if (tv[1].tv_nsec == UTIME_OMIT)
    {
        return;
    }

    m_subvfs.utimens(path, tv);

    // for UTIME_NOW we need to get the precise remote time, can't just use tv[1]
    // TODO let the command return the stat
    struct stat st{};
    m_subvfs.getattr(path, st);

    auto lg = m_cache.lock();
    m_cache.get_node(path).st = st;
}

//--------------------------------------------------------------------------

void CachedVfs::truncate(const Path& path, const off_t length)
{
    m_subvfs.truncate(path, length);

    // TODO let the create command return the stat
    struct stat st{};
    m_subvfs.getattr(path, st);

    auto lg = m_cache.lock();
    auto& node = m_cache.get_node(path);
    node.st = st;
}

//--------------------------------------------------------------------------

IVfs::FileHandle CachedVfs::create(const Path& path, const int flags, const mode_t mode)
{
    const auto subvfs_handle = m_subvfs.create(path, flags, mode);

    // TODO let the create command return the stat
    struct stat st{};
    m_subvfs.getattr(path, st);

    auto lg = m_cache.lock();
    m_cache.make_node(path).st = st;
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
    auto lg = m_cache.lock();
    const auto handle = FileHandle{m_id_dispenser.get()};
    m_opened_files.emplace(std::make_pair(handle, std::move(file)));
    return handle;
}

//--------------------------------------------------------------------------

void CachedVfs::close(const FileHandle fh)
{
    auto lg = m_cache.lock();
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
    auto lg = m_cache.lock();
    const auto it = m_opened_files.find(fh);
    if (it == m_opened_files.end())
    {
        throw std::system_error{EBADF, std::generic_category()};
    }

    bool has_cached_block = m_cache.read(
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
        m_cache.write(refreshed_it->second.path, static_cast<uintmax_t>(offset),
                      {output.begin(), output.end()});
        return ret;
    }
}

//--------------------------------------------------------------------------

size_t CachedVfs::write(const FileHandle fh, const gsl::span<const uint8_t> input,
                        const off_t offset)
{
    auto lg = m_cache.lock();
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

    auto& node = m_cache.get_node(file.path);
    node.st = st;
    m_cache.write(file.path, static_cast<uintmax_t>(offset),
                  {input.begin(), input.end()});

    return res;
}

//==========================================================================

BackgroundLoader::BackgroundLoader(Serializer& serializer, Deserializer& deserializer,
                                   Distributor& distributor, cache::Cache& cache)
    : m_serializer{serializer}
    , m_deserializer{deserializer}
    , m_distributor{distributor}
    , m_cache{cache}
{
#define SUB(_Msg, func) \
    m_distributor.subscribe<messages::_Msg>( \
        [this](const MessageId, const auto& msg) { \
            BackgroundLoader::func(msg); \
        });
    SUB(NotifyChanged, process_remote_changed);
}

//--------------------------------------------------------------------------

void BackgroundLoader::start()
{
    m_tree_loader_thread = std::thread{&BackgroundLoader::tree_loader, this};
}

//--------------------------------------------------------------------------

void BackgroundLoader::stop()
{
    m_quit = true;
}

//--------------------------------------------------------------------------

void BackgroundLoader::wait()
{
    if (m_tree_loader_thread.joinable())
    {
        m_tree_loader_thread.join();
    }
}

//--------------------------------------------------------------------------

void BackgroundLoader::invalidate_tree()
{
    m_tree_invalidated = true;
    m_cv.notify_one();
}

//--------------------------------------------------------------------------

void BackgroundLoader::populate_tree()
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

    auto lg = m_cache.lock();
    m_cache.reset();
    populate_tree(m_cache.get_root(), *message.tree());

    log_info("populating tree done");
}

//--------------------------------------------------------------------------

void BackgroundLoader::populate_tree(cache::Node& node, const messages::TreeNode& fbb_node)
{
    node.name = fbb_node.name()->str();
    copy(*fbb_node.st(), node.st);
    for (const auto& child: *fbb_node.children())
    {
        auto& new_child = m_cache.make_node(node, child->name()->str());
        populate_tree(new_child, *child);
    }
}

//--------------------------------------------------------------------------

static size_t block_aligned_size(const size_t sz)
{
    static constexpr size_t BLKSIZE{4096};
    return ((sz + BLKSIZE - 1) / BLKSIZE) * BLKSIZE;
}

//--------------------------------------------------------------------------

template<typename _It>
void BackgroundLoader::preload_files_bulks(const _It begin, const _It end)
{
    try
    {
        auto queue = m_serializer.new_queue(Serializer::PRIORITY_BACKGROUND);
        std::vector<MessageId> mids{};

        const auto wait_for_mids = [this, &mids]()
        {
            log_trace("waiting for a batch");
            for (const auto mid: mids)
            {
                const auto res
                    = m_deserializer.wait_for_result<messages::ResultPreread>(mid, TIMEOUT);
                if (not res.is_valid())
                {
                    throw std::system_error{EHOSTUNREACH, std::generic_category()};
                }
                const auto& message = res.message();
                if (message.res() < 0)
                {
                    // silentely ignore the read error
                    log_trace("preloading failed {} errno:{}",
                              message.path()->c_str(), message.res_errno());
                    continue;
                }

                // FUSE reads are 4k block aligned
                std::vector<uint8_t> buf(block_aligned_size(message.data()->size()));
                assert(buf.size() >= message.data()->size());
                std::copy(message.data()->begin(), message.data()->end(), buf.begin());
                m_cache.write(message.path()->str(), message.offset(), std::move(buf));
            }
        };

        // a size limit for a send buffer on the server side
        static constexpr uint64_t BULK_SIZE{1024 * 1024};
        uint64_t size_counter{0};
        for (auto files_it = begin; files_it != end; ++files_it)
        {
            log_trace("preloading {}", files_it->path.native());
            uint64_t offset{0};
            while (offset < files_it->size)
            {
                flatbuffers::FlatBufferBuilder fbb{};
                const auto blk_size
                    = std::min(IVfs::IO_FRAGMENT_SIZE, files_it->size - offset);
                const auto command = messages::CreateCommandPrereadDirect(
                    fbb, files_it->path.c_str(), static_cast<size_t>(offset),
                    blk_size);
                mids.emplace_back(m_serializer.add_command(queue, fbb, command));
                offset += IVfs::IO_FRAGMENT_SIZE;

                size_counter += blk_size;
                if (size_counter >= BULK_SIZE)
                {
                    wait_for_mids();
                    mids.clear();
                    size_counter = 0;
                }
            }
        }
        wait_for_mids();
    }
    catch (const std::exception& exc)
    {
        log_error("preload failed: {}", exc.what());
    }
}

//--------------------------------------------------------------------------

void BackgroundLoader::preload_files()
{
    static const std::array<std::regex, 1> patterns{{
        //std::regex{".*"}
        std::regex{".*/.gitignore"}
    }};

    log_info("preloading content");

    auto lg = m_cache.lock();
    std::vector<FileInfo> files_list{};
    const auto browser
        = [&files_list](const IVfs::Path& parent_, const cache::Node& node_) -> void
    {
        const auto browser_impl
            = [&files_list](const IVfs::Path& parent, const cache::Node& node,
                            auto& browser_ref) -> void
        {
            const IVfs::Path node_path{(parent == "") ? "/" : parent / node.name};
            const auto is_directory
                = static_cast<bool>((node.st.st_mode & S_IFDIR) == S_IFDIR);
            const auto is_symlink
                = static_cast<bool>((node.st.st_mode & S_IFLNK) == S_IFLNK);
            if (not is_directory and not is_symlink)
            {
                for (const auto& pat: patterns)
                {
                    if (std::regex_match(node_path.native(), pat))
                    {
                        files_list.push_back(
                            {node_path, static_cast<uint64_t>(node.st.st_size)});
                        break;
                    }
                }
            }
            for (const auto& child : node.children)
            {
                browser_ref(node_path, child.second, browser_ref);
            }
        };
        browser_impl(parent_, node_, browser_impl);
    };
    browser("", m_cache.get_root());

    preload_files_bulks(files_list.begin(), files_list.end());

    log_info("preloading content done");
}

//--------------------------------------------------------------------------

void BackgroundLoader::tree_loader()
{
    while (not m_quit)
    {
        auto lg = m_cache.lock();
        m_cv.wait(lg, [this]() { return m_tree_invalidated.load(); });
        lg.unlock();

        if (m_tree_invalidated.load())
        {
            m_tree_invalidated = false;
            try
            {
                populate_tree();
                preload_files();
            }
            catch (const std::exception& err)
            {
                log_error("{}", err.what());
            }
        }
    }
}

//--------------------------------------------------------------------------

void BackgroundLoader::process_remote_changed(const messages::NotifyChanged&)
{
    invalidate_tree();
}

//==========================================================================
} // namespace rewofs::client
