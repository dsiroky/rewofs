/// Messages support.
///
/// @file

#pragma once
#ifndef MESSAGES_HPP__78PIAARG
#define MESSAGES_HPP__78PIAARG

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rewofs/messages/all.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

inline void copy(const timespec& src, messages::Time& dst)
{
    dst.mutate_nsec(src.tv_nsec);
    dst.mutate_sec(src.tv_sec);
}

//--------------------------------------------------------------------------

inline void copy(const messages::Time& src, timespec& dst)
{
    dst.tv_nsec = src.nsec();
    dst.tv_sec = src.sec();
}

//--------------------------------------------------------------------------

inline void copy(struct stat& src, messages::Stat& dst)
{
    dst.mutate_st_mode(src.st_mode);
    dst.mutate_st_size(src.st_size);
    copy(src.st_ctim, dst.mutable_st_ctim());
    copy(src.st_mtim, dst.mutable_st_mtim());
}

//--------------------------------------------------------------------------

inline void copy(const messages::Stat& src, struct stat& dst)
{
    dst.st_mode = src.st_mode();
    dst.st_size = src.st_size();
    copy(src.st_ctim(), dst.st_ctim);
    copy(src.st_mtim(), dst.st_mtim);
}

//==========================================================================
} // namespace rewofs

#endif /* include guard */
