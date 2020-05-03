/// @copydoc worker.hpp
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <boost/range/iterator_range.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/log.hpp"
#include "rewofs/messages.hpp"
#include "rewofs/path.hpp"
#include "rewofs/server/worker.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

namespace fs = boost::filesystem;

//==========================================================================
namespace {
//==========================================================================

#ifndef RENAME_EXCHANGE
#define RENAME_NOREPLACE (1 << 0)
#define RENAME_EXCHANGE (1 << 1)
// ugly replacement for older GLIBC
int renameat2(int olddirfd, const char* oldpath, int newdirfd, const char* newpath,
              unsigned int flags)
{
    if (flags & RENAME_EXCHANGE)
    {
        const auto tmp = std::string{oldpath} + ".498560w354df7w";
        int res{};
        res = renameat(olddirfd, oldpath, olddirfd, tmp.c_str());
        if (res != 0)
        {
            return res;
        }
        res = renameat(newdirfd, newpath, olddirfd, oldpath);
        if (res != 0)
        {
            return res;
        }
        res = renameat(olddirfd, tmp.c_str(), newdirfd, newpath);
        if (res != 0)
        {
            return res;
        }
        return 0;
    }
    else
    {
        if (flags & RENAME_NOREPLACE)
        {
            if (fs::exists(newpath))
            {
                return EEXIST;
            }
            return renameat(olddirfd, oldpath, newdirfd, newpath);
        }
        else
        {
            return renameat(olddirfd, oldpath, newdirfd, newpath);
        }
    }
}
#endif

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::TreeNode>
    build_fs_fbb_tree(flatbuffers::FlatBufferBuilder& fbb, const fs::path& path)
{
    // TODO limit depth (possible loops via e.g. mount -oloop)

    struct stat st{};
    const auto res = lstat(path.c_str(), &st);
    messages::Stat fbb_stat{};
    if (res == 0)
    {
        copy(st, fbb_stat);
    }
    else
    {
        log_warning("{} {} {}", path.native(), res, errno);
    }

    std::vector<flatbuffers::Offset<messages::TreeNode>> vec_children{};
    try
    {
        if (fs::is_directory(path))
        {
            const auto children
                = boost::make_iterator_range(fs::directory_iterator{path}, {});
            for (const auto& child : children)
            {
                vec_children.push_back(build_fs_fbb_tree(fbb, child.path()));
            }
        }
    }
    catch (const std::exception& exc)
    {
        log_warning("{} {}", path.native(), exc.what());
    }

    const auto fbb_name = fbb.CreateString(path.filename().native());
    const auto fbb_children = fbb.CreateVector(vec_children);
    messages::TreeNodeBuilder builder{fbb};
    builder.add_name(fbb_name);
    builder.add_st(&fbb_stat);
    builder.add_children(fbb_children);
    return builder.Finish();
}

//==========================================================================
} // namespace
//==========================================================================

Worker::Worker(server::Transport& transport, TemporalIgnores& temporal_ignores)
    : m_transport{transport}, m_temporal_ignores{temporal_ignores}
{
#define SUB(_Msg, func) \
    m_distributor.subscribe<messages::_Msg>( \
        [this](const MessageId mid, const auto& msg) { \
            process_message(mid, msg, &Worker::func); \
        });
    SUB(Ping, process_ping);
    SUB(CommandReadTree, process_read_tree);
    SUB(CommandStat, process_stat);
    SUB(CommandReaddir, process_readdir);
    SUB(CommandReadlink, process_readlink);
    SUB(CommandMkdir, process_mkdir);
    SUB(CommandRmdir, process_rmdir);
    SUB(CommandUnlink, process_unlink);
    SUB(CommandSymlink, process_symlink);
    SUB(CommandRename, process_rename);
    SUB(CommandChmod, process_chmod);
    SUB(CommandTruncate, process_truncate);
    SUB(CommandOpen, process_open);
    SUB(CommandClose, process_close);
    SUB(CommandRead, process_read);
    SUB(CommandWrite, process_write);
}

//--------------------------------------------------------------------------

void Worker::start()
{
    for (auto& thr: m_threads)
    {
        thr = std::thread{&Worker::run, this};
    }

    m_recv_thread = std::thread(&Worker::recv_loop, this);
}

//--------------------------------------------------------------------------

void Worker::stop()
{
    m_quit = true;
    m_requests_queue.stop();
}

//--------------------------------------------------------------------------

void Worker::wait()
{
    for (auto& thr: m_threads)
    {
        if (thr.joinable())
        {
            thr.join();
        }
    }

    if (m_recv_thread.joinable())
    {
        m_recv_thread.join();
    }
}

//--------------------------------------------------------------------------

void Worker::recv_loop()
{
    while (not m_quit)
    {
        m_transport.recv([this](const gsl::span<const uint8_t> buf) {
            m_requests_queue.push({buf.begin(), buf.end()});
        });
    }
}

//--------------------------------------------------------------------------

void Worker::run()
{
    while (not m_quit)
    {
        try
        {
            const auto buf = m_requests_queue.pop();
            m_distributor.process_frame(buf);
        }
        catch (const QuitSignal&)
        {
            break;
        }
    }
}

//--------------------------------------------------------------------------

void Worker::temporal_ignore(const std::string& path)
{
    m_temporal_ignores.add(std::chrono::steady_clock::now(), path);
}

//--------------------------------------------------------------------------

template<typename _Msg, typename _ProcFunc>
void Worker::process_message(const MessageId mid, const _Msg& msg, _ProcFunc proc)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto res = (this->*proc)(fbb, msg);
    const auto frame = make_frame(fbb, strong::value_of(mid), res);
    fbb.Finish(frame);
    log_trace("reply mid:{}", strong::value_of(mid));
    m_transport.send({fbb.GetBufferPointer(), fbb.GetSize()});
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::Pong>
    Worker::process_ping(flatbuffers::FlatBufferBuilder& fbb, const messages::Ping&)
{
    return messages::CreatePong(fbb);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultReadTree>
    Worker::process_read_tree(flatbuffers::FlatBufferBuilder& fbb,
                              const messages::CommandReadTree& msg)
{
    const auto path = map_path(msg.path()->c_str());
    log_trace("read tree {}", path.native());

    auto tree = build_fs_fbb_tree(fbb, path);
    auto res_builder = messages::ResultReadTreeBuilder{fbb};
    res_builder.add_res_errno(0);
    res_builder.add_tree(tree);
    const auto ret = res_builder.Finish();
    log_trace("tree msg size ~ {}", ret.o);
    return ret;
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultStat>
    Worker::process_stat(flatbuffers::FlatBufferBuilder& fbb,
                         const messages::CommandStat& msg)
{
    const auto path = map_path(msg.path()->c_str());

    struct stat st{};
    const auto res = lstat(path.c_str(), &st);
    log_trace("{} res:{}", path.native(), res);

    auto res_builder = messages::ResultStatBuilder{fbb};
    if (res == 0)
    {
        res_builder.add_res_errno(0);
        messages::Stat msg_st{};
        copy(st, msg_st);
        res_builder.add_st(&msg_st);
    }
    else
    {
        res_builder.add_res_errno(errno);
    }

    return res_builder.Finish();
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultReaddir>
    Worker::process_readdir(flatbuffers::FlatBufferBuilder& fbb,
                            const messages::CommandReaddir& msg)
{
    const auto path = map_path(msg.path()->c_str());
    log_trace("{}", path.native());

    std::vector<flatbuffers::Offset<messages::TreeNode>> items{};

    try
    {
        const fs::directory_iterator end_itr{};
        for (fs::directory_iterator itr{path}; itr != end_itr; ++itr)
        {
            const auto fs_item_path = itr->path().filename();
            const auto item_path = fbb.CreateString(fs_item_path.native());
            struct stat st{};
            const auto res = stat(fs_item_path.c_str(), &st);
            if (res == 0)
            {
                messages::Stat msg_st{};
                copy(st, msg_st);
                const auto item = messages::CreateTreeNode(fbb, item_path, &msg_st);
                items.push_back(item);
            }
            else
            {
                const auto item = messages::CreateTreeNode(fbb, item_path, nullptr);
                items.push_back(item);
            }
        }
        const auto fb_items = fbb.CreateVector(items);
        return messages::CreateResultReaddir(fbb, 0, fb_items);
    }
    catch (const fs::filesystem_error& exc)
    {
        return messages::CreateResultReaddir(fbb, exc.code().value());
    }
    catch (const std::exception& exc)
    {
        log_error("{}", exc.what());
        return messages::CreateResultReaddir(fbb, EIO);
    }
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultReadlink>
    Worker::process_readlink(flatbuffers::FlatBufferBuilder& fbb,
                             const messages::CommandReadlink& msg)
{
    const auto path = map_path(msg.path()->c_str());

    try
    {
        const auto resolved = fs::read_symlink(path);
        return messages::CreateResultReadlinkDirect(fbb, 0, resolved.c_str());
    }
    catch (const fs::filesystem_error& exc)
    {
        return messages::CreateResultReadlink(fbb, exc.code().value());
    }
    catch (const std::exception& exc)
    {
        log_error("{}", exc.what());
        return messages::CreateResultReadlink(fbb, EIO);
    }
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_mkdir(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandMkdir& msg)
{
    temporal_ignore(msg.path()->str());
    const auto path = map_path(msg.path()->c_str());

    const auto res = mkdir(path.c_str(), msg.mode());
    log_trace("{} res:{}", path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_rmdir(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandRmdir& msg)
{
    temporal_ignore(msg.path()->str());
    const auto path = map_path(msg.path()->c_str());

    const auto res = rmdir(path.c_str());
    log_trace("{} res:{}", path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_unlink(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandUnlink& msg)
{
    temporal_ignore(msg.path()->str());
    const auto path = map_path(msg.path()->c_str());

    const auto res = unlink(path.c_str());
    log_trace("{} res:{}", path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_symlink(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandSymlink& msg)
{
    temporal_ignore(msg.link_path()->str());
    const auto link_path = map_path(msg.link_path()->c_str());

    const auto res = symlink(msg.target()->c_str(), link_path.c_str());
    log_trace("{}->{} res:{}", link_path.native(), msg.target()->c_str(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_rename(flatbuffers::FlatBufferBuilder& fbb,
                           const messages::CommandRename& msg)
{
    temporal_ignore(msg.old_path()->str());
    temporal_ignore(msg.new_path()->str());
    const auto old_path = map_path(msg.old_path()->c_str());
    const auto new_path = map_path(msg.new_path()->c_str());
    const auto flags = msg.flags();

    const auto res
        = renameat2(AT_FDCWD, old_path.c_str(), AT_FDCWD, new_path.c_str(), flags);
    log_trace("{}->{} res:{}", old_path.native(), new_path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_chmod(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandChmod& msg)
{
    temporal_ignore(msg.path()->str());
    const auto path = map_path(msg.path()->c_str());

    const auto res = chmod(path.c_str(), msg.mode());
    log_trace("{} res:{}", path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_truncate(flatbuffers::FlatBufferBuilder& fbb,
                             const messages::CommandTruncate& msg)
{
    temporal_ignore(msg.path()->str());
    const auto path = map_path(msg.path()->c_str());

    const auto res = truncate(path.c_str(), static_cast<off_t>(msg.lenght()));
    log_trace("{} res:{}", path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_open(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandOpen& msg)
{
    temporal_ignore(msg.path()->str());
    const auto path = map_path(msg.path()->c_str());

    int res{-1};
    if (flatbuffers::IsFieldPresent(&msg, messages::CommandOpen::VT_MODE))
    {
        res = open(path.c_str(), msg.flags(), msg.mode());
    }
    else
    {
        res = open(path.c_str(), msg.flags());
    }
    log_trace("{} fh:{} fd:{}", path.native(), msg.file_handle(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    {
        std::lock_guard lg{m_mutex};
        assert(m_opened_files.find(msg.file_handle()) == m_opened_files.end());
        m_opened_files[msg.file_handle()].fd = res;
    }
    assert(get_file_descriptor(msg.file_handle()).first.fd == res);
    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

std::pair<Worker::FileRef, std::unique_lock<std::mutex>>
    Worker::get_file_descriptor(const uint64_t fh)
{
    std::lock_guard lg{m_mutex};
    const auto it = m_opened_files.find(fh);
    if (it == m_opened_files.end())
    {
        return std::make_pair(FileRef{-1, std::nullopt}, std::unique_lock<std::mutex>{});
    }
    return std::make_pair(FileRef{it->second.fd, it->second.path},
                          std::unique_lock<std::mutex>{it->second.mutex});
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_close(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandClose& msg)
{
    auto [file_ref, flguard] = get_file_descriptor(msg.file_handle());

    if (file_ref.is_valid())
    {
        temporal_ignore(*file_ref.path);
    }

    const auto res = close(file_ref.fd);
    log_trace("fd:{} res:{}", file_ref.fd, res);

    flguard.unlock();

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    std::lock_guard lg{m_mutex};
    m_opened_files.erase(m_opened_files.find(msg.file_handle()));
    return messages::CreateResultErrno(fbb, 0);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultRead>
    Worker::process_read(flatbuffers::FlatBufferBuilder& fbb,
                         const messages::CommandRead& msg)
{
    auto [file_ref, flguard] = get_file_descriptor(msg.file_handle());

    const auto new_ofs = lseek(file_ref.fd, static_cast<off_t>(msg.offset()), SEEK_SET);
    if (new_ofs != static_cast<off_t>(msg.offset()))
    {
        return messages::CreateResultReadDirect(fbb, -1, static_cast<int>(new_ofs));
    }

    std::vector<uint8_t> buffer(msg.size());
    const auto res = read(file_ref.fd, buffer.data(), msg.size());
    log_trace("fd:{} res:{}", file_ref.fd, res);

    flguard.unlock();

    if (res < 0)
    {
        return messages::CreateResultReadDirect(fbb, res, errno);
    }

    const auto data = fbb.CreateVector(buffer.data(), static_cast<size_t>(res));
    return messages::CreateResultRead(fbb, res, 0, data);
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultWrite>
    Worker::process_write(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandWrite& msg)
{
    auto [file_ref, flguard] = get_file_descriptor(msg.file_handle());

    if (file_ref.is_valid())
    {
        temporal_ignore(*file_ref.path);
    }

    const auto new_ofs = lseek(file_ref.fd, static_cast<off_t>(msg.offset()), SEEK_SET);
    if (new_ofs != static_cast<off_t>(msg.offset()))
    {
        return messages::CreateResultWrite(fbb, -1, static_cast<int>(new_ofs));
    }

    const auto res = write(file_ref.fd, msg.data()->data(), msg.data()->size());
    log_trace("fd:{} res:{}", file_ref.fd, res);

    flguard.unlock();

    if (res < 0)
    {
        return messages::CreateResultWrite(fbb, res, errno);
    }

    return messages::CreateResultWrite(fbb, res, 0);
}

//==========================================================================
} // namespace rewofs::server
