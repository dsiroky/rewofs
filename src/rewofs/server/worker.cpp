/// @copydoc worker.hpp
///
/// @file

#include <sys/stat.h>

#include "rewofs/log.hpp"
#include "rewofs/server/worker.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

namespace fs = boost::filesystem;

//==========================================================================
namespace {
//==========================================================================

void copy(const timespec& src, messages::Time& dst)
{
    dst.mutate_nsec(src.tv_nsec);
    dst.mutate_sec(src.tv_sec);
}

void copy(struct stat& src, messages::Stat& dst)
{
    dst.mutate_st_mode(src.st_mode);
    dst.mutate_st_nlink(src.st_nlink);
    copy(src.st_atim, dst.mutable_st_atim());
    copy(src.st_ctim, dst.mutable_st_ctim());
    copy(src.st_mtim, dst.mutable_st_mtim());
}

//==========================================================================
} // namespace
//==========================================================================

Worker::Worker(server::Transport& transport)
    : m_transport{transport}
{
    m_served_directory = boost::filesystem::current_path();

#define SUB(_Msg, func) \
    m_distributor.subscribe<messages::_Msg>( \
        [this](const MessageId mid, const auto& msg) { \
            process_message(mid, msg, &Worker::func); \
        });
    SUB(CommandStat, process_stat);
    SUB(CommandReaddir, process_readdir);
    SUB(CommandReadlink, process_readlink);
    SUB(CommandMkdir, process_mkdir);
    SUB(CommandRmdir, process_rmdir);
    SUB(CommandUnlink, process_unlink);
    SUB(CommandSymlink, process_symlink);
    SUB(CommandRename, process_rename);
    SUB(CommandChmod, process_chmod);
}

//--------------------------------------------------------------------------

void Worker::start()
{
    m_thread = std::thread{&Worker::run, this};
}

//--------------------------------------------------------------------------

void Worker::wait()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

//--------------------------------------------------------------------------

boost::filesystem::path Worker::map_path(const boost::filesystem::path& relative)
{
    // TODO check for out-of-direcotory values like ".."
    return fs::absolute(fs::path{"./"} / relative).lexically_normal();
}

//--------------------------------------------------------------------------

void Worker::run()
{
    for (;;)
    {
        m_transport.recv([this](const gsl::span<const uint8_t> buf) {
            m_distributor.process_frame(buf);
        });
    }
}

//--------------------------------------------------------------------------

template<typename _Msg, typename _ProcFunc>
void Worker::process_message(const MessageId mid, const _Msg& msg, _ProcFunc proc)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto res = (this->*proc)(fbb, msg);
    const auto frame = make_frame(fbb, strong::value_of(mid), res);
    fbb.Finish(frame);
    m_transport.send({fbb.GetBufferPointer(), fbb.GetSize()});
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

    std::vector<flatbuffers::Offset<messages::DirectoryItem>> items{};

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
                const auto item = messages::CreateDirectoryItem(fbb, item_path, &msg_st);
                items.push_back(item);
            }
            else
            {
                const auto item = messages::CreateDirectoryItem(fbb, item_path, nullptr);
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
    catch (...)
    {
        return messages::CreateResultReaddir(fbb, EIO);
    }
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultReadlink>
    Worker::process_readlink(flatbuffers::FlatBufferBuilder& fbb,
                             const messages::CommandReadlink& msg)
{
    const auto path = map_path(msg.path()->c_str());

    std::string buf(1024, ' ');
    const auto res = readlink(path.c_str(), buf.data(), buf.size());
    log_trace("{} res:{}", path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultReadlink(fbb, errno);
    }

    buf = buf.substr(0, static_cast<size_t>(res));
    return messages::CreateResultReadlink(fbb, 0, fbb.CreateString(buf));
}

//--------------------------------------------------------------------------

flatbuffers::Offset<messages::ResultErrno>
    Worker::process_mkdir(flatbuffers::FlatBufferBuilder& fbb,
                          const messages::CommandMkdir& msg)
{
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
    const auto old_path = map_path(msg.old_path()->c_str());
    const auto new_path = map_path(msg.new_path()->c_str());

    const auto res = rename(old_path.c_str(), new_path.c_str());
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
    const auto path = map_path(msg.path()->c_str());

    const auto res = chmod(path.c_str(), msg.mode());
    log_trace("{} res:{}", path.native(), res);

    if (res < 0)
    {
        return messages::CreateResultErrno(fbb, errno);
    }

    return messages::CreateResultErrno(fbb, 0);
}

//==========================================================================
} // namespace rewofs::server
