/// Path utilities.
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include "rewofs/enablewarnings.hpp"

//==========================================================================
namespace rewofs::server {
//==========================================================================

/// @return absolute path from a relative path to the served directory
boost::filesystem::path map_path(const boost::filesystem::path& relative);

//==========================================================================
} // namespace rewofs::server
