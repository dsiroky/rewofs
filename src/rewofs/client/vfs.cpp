/// @copydoc vfs.hpp
///
/// @file

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

void copy(const messages::Stat& src, struct stat& dst)
{
    dst.st_mode = src.st_mode();
    dst.st_nlink = src.st_nlink();
    dst.st_size = src.st_size();
    copy(src.st_atim(), dst.st_atim);
    copy(src.st_ctim(), dst.st_ctim);
    copy(src.st_mtim(), dst.st_mtim);
}

//==========================================================================
} // namespace
//==========================================================================

RemoteVfs::RemoteVfs(Serializer& serializer, Deserializer& deserializer)
    : m_serializer{serializer}
    , m_deserializer{deserializer}
{
}

//--------------------------------------------------------------------------

void RemoteVfs::set_seed(const uint64_t seed)
{
    m_open_id_dispenser = seed;
}

//--------------------------------------------------------------------------

void RemoteVfs::getattr(const Path& path, struct stat& st)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandStatDirect(fbb, path.c_str());
    const auto res = single_command<messages::ResultStat>(fbb, command);

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
    const auto res = single_command<messages::ResultReaddir>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    for (const auto& item: *message.items())
    {
        if (item->st() == nullptr)
        {
            filler(item->path()->str(), {});
        }
        else
        {
            struct stat st{};
            copy(*item->st(), st);
            filler(item->path()->str(), st);
        }
    }
}

//--------------------------------------------------------------------------

std::string RemoteVfs::readlink(const Path& path)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandReadlinkDirect(fbb, path.c_str());
    const auto res = single_command<messages::ResultReadlink>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    return message.path()->str();
}

//--------------------------------------------------------------------------

void RemoteVfs::mkdir(const Path& path, mode_t mode)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandMkdirDirect(fbb, path.c_str(), mode);
    const auto res = single_command<messages::ResultErrno>(fbb, command);

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
    const auto res = single_command<messages::ResultErrno>(fbb, command);

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
    const auto res = single_command<messages::ResultErrno>(fbb, command);

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
    const auto res = single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::rename(const Path& old_path, const Path& new_path)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandRenameDirect(fbb, old_path.c_str(),
                                                             new_path.c_str());
    const auto res = single_command<messages::ResultErrno>(fbb, command);

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
    const auto res = single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

void RemoteVfs::truncate(const Path& path, const off_t lenght)
{
    if (lenght < 0)
    {
        throw std::system_error{EINVAL, std::generic_category()};
    }

    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandTruncateDirect(
        fbb, path.c_str(), static_cast<uint64_t>(lenght));
    const auto res = single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

uint64_t RemoteVfs::open_common(const Path& path, const int flags,
                                const std::optional<mode_t> mode)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto new_open_id = m_open_id_dispenser++;
    const messages::OpenParams open_params{flags, mode.has_value(), mode.value_or(0)};
    const auto command = messages::CreateCommandOpenDirect(
        fbb, path.c_str(), strong::value_of(new_open_id), &open_params);
    const auto res = single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    std::lock_guard lg{m_mutex};
    FileParams fparams{};
    fparams.path = path;
    fparams.open_params.flags = flags;
    fparams.open_params.mode = mode;
    m_opened_files.emplace(std::make_pair(FileHandle{new_open_id}, fparams));
    return new_open_id;
}

//--------------------------------------------------------------------------

std::pair<const uint64_t, messages::OpenParams>
    RemoteVfs::get_file_msg_params(const FileHandle fh)
{
    const auto it = m_opened_files.find(fh);
    if (it == m_opened_files.end())
    {
        throw std::system_error{EINVAL, std::generic_category()};
    }
    const auto& open_params = it->second.open_params;
    const messages::OpenParams msg_open_params{
        open_params.flags, open_params.mode.has_value(), open_params.mode.value_or(0)};
    return {strong::value_of(fh), msg_open_params};
}

//--------------------------------------------------------------------------

uint64_t RemoteVfs::open(const Path& path, const int flags, const mode_t mode)
{
    return open_common(path, flags, mode);
}

//--------------------------------------------------------------------------

uint64_t RemoteVfs::open(const Path& path, const int flags)
{
    return open_common(path, flags, std::nullopt);
}

//--------------------------------------------------------------------------

void RemoteVfs::close(const FileHandle fh)
{
    std::lock_guard lg{m_mutex};
    const auto it = m_opened_files.find(fh);
    if (it == m_opened_files.end())
    {
        throw std::system_error{EINVAL, std::generic_category()};
    }

    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandClose(fbb, strong::value_of(fh));
    const auto res = single_command<messages::ResultErrno>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//--------------------------------------------------------------------------

ssize_t RemoteVfs::read(const FileHandle fh, const gsl::span<uint8_t> output,
                        const off_t offset)
{
    std::lock_guard lg{m_mutex};
    auto [raw_fh, msg_open_params] = get_file_msg_params(fh);

    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = messages::CreateCommandRead(fbb, raw_fh, offset, output.size(),
                                                     &msg_open_params);
    const auto res = single_command<messages::ResultRead>(fbb, command);

    const auto& message = res.message();
    if (message.res() < 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    std::copy(message.data()->begin(), message.data()->end(), output.begin());
    return message.data()->size();
}

//--------------------------------------------------------------------------

ssize_t RemoteVfs::write(const FileHandle fh, const gsl::span<const uint8_t> input,
                         const off_t offset)
{
    std::lock_guard lg{m_mutex};
    auto [raw_fh, msg_open_params] = get_file_msg_params(fh);

    flatbuffers::FlatBufferBuilder fbb{};
    const auto data = fbb.CreateVector(input.data(), input.size());
    const auto command
        = messages::CreateCommandWrite(fbb, raw_fh, offset, data, &msg_open_params);
    const auto res = single_command<messages::ResultWrite>(fbb, command);

    const auto& message = res.message();
    if (message.res() < 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }

    return message.res();
}

//==========================================================================
} // namespace rewofs::client
