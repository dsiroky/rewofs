/// Test caches.
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <gtest/gtest.h>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/client/cache.hpp"

//==========================================================================
namespace rewofs::tests {
//==========================================================================

TEST(CacheTree, GetNode_Root)
{
    client::cache::Tree tree{};
    EXPECT_NO_THROW(tree.get_node("/"));
}

//--------------------------------------------------------------------------

TEST(CacheTree, GetNode_NonexistentInRoot)
{
    client::cache::Tree tree{};
    EXPECT_THROW(tree.get_node("/nonexistent"), std::system_error);
}

//--------------------------------------------------------------------------

TEST(CacheTree, MakeNode_GetNode)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    auto& some = tree.make_node(root, "some");
    EXPECT_NO_THROW(tree.get_node("/some"));
    tree.make_node(root, "some2");
    EXPECT_NO_THROW(tree.get_node("/some2"));
    tree.make_node(some, "sub");
    EXPECT_NO_THROW(tree.get_node("/some/sub"));
    EXPECT_THROW(tree.get_node("/some/sub2"), std::system_error);
}

//==========================================================================
} // namespace rewofs::tests

