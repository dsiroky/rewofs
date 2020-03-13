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

void RemoteVfs::getattr(const Path& path, struct stat& st)
{
    flatbuffers::FlatBufferBuilder fbb{};
    const auto command = make_command_stat(fbb, path.native());
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
    const auto command = make_command_readdir(fbb, path.native());
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
    const auto command = make_command_readlink(fbb, path.native());
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
    const auto res = single_command<messages::ResultMkdir>(fbb, command);

    const auto& message = res.message();
    if (message.res_errno() != 0)
    {
        throw std::system_error{message.res_errno(), std::generic_category()};
    }
}

//==========================================================================
} // namespace rewofs::client
