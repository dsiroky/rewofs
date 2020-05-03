/// Path utilities.
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include "rewofs/enablewarnings.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

/// Item in a breadth-first search list.
struct BreadthDirectoryItem
{
    boost::filesystem::directory_entry entry{};
    size_t children_count{0};

    bool operator==(const BreadthDirectoryItem& other) const
    {
        return (entry == other.entry) and (children_count == other.children_count);
    }
};

//==========================================================================

std::vector<BreadthDirectoryItem> breadth_first_tree(const boost::filesystem::path& root_path);
/// @return absolute path from a relative path to the current directory
boost::filesystem::path map_path(const boost::filesystem::path& relative);

//==========================================================================
} // namespace rewofs::server
