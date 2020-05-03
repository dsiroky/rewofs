/// @copydoc path.hpp
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <boost/range/iterator_range.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/log.hpp"
#include "rewofs/path.hpp"

//==========================================================================
namespace rewofs {
//==========================================================================

std::vector<BreadthDirectoryItem>
    breadth_first_tree(const boost::filesystem::path& root_path)
{
    namespace fs = boost::filesystem;

    std::vector<BreadthDirectoryItem> list{};
    BreadthDirectoryItem root{
        fs::directory_entry{root_path, fs::symlink_status(root_path)}, 0};
    list.push_back(std::move(root));

    size_t current_idx{0};
    while (current_idx < list.size())
    {
        auto& parent = list[current_idx];
        if (parent.entry.symlink_status().type() == fs::directory_file)
        {
            std::vector<BreadthDirectoryItem> tmp_list{};
            const auto children = boost::make_iterator_range(
                fs::directory_iterator{parent.entry.path()}, {});
            for (const auto& child : children)
            {
                BreadthDirectoryItem child_item{
                    fs::directory_entry{std::move(child.path()), child.symlink_status()},
                    0};
                tmp_list.push_back(std::move(child_item));
            }

            parent.children_count = static_cast<uint32_t>(tmp_list.size());
            std::move(tmp_list.begin(), tmp_list.end(), std::back_inserter(list));
        }

        ++current_idx;
    }

    return list;
}

//--------------------------------------------------------------------------

boost::filesystem::path map_path(const boost::filesystem::path& relative)
{
    namespace fs = boost::filesystem;
    // TODO check for out-of-direcotory values like ".."
    return fs::absolute(fs::path{"./"} / relative).lexically_normal();
}

//==========================================================================
} // namespace rewofs::server
