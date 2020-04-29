/// @copydoc server/path.hpp
///
/// @file

#include "rewofs/server/path.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

boost::filesystem::path map_path(const boost::filesystem::path& relative)
{
    namespace fs = boost::filesystem;
    // TODO check for out-of-direcotory values like ".."
    return fs::absolute(fs::path{"./"} / relative).lexically_normal();
}

//==========================================================================
} // namespace rewofs::server
