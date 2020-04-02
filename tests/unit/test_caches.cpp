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

TEST(CacheTree, Lstat_Root)
{
    client::cache::Tree tree{};
    EXPECT_NO_THROW(tree.lstat("/"));
}

//--------------------------------------------------------------------------

TEST(CacheTree, Lstat_NonexistentInRoot)
{
    client::cache::Tree tree{};
    EXPECT_THROW(tree.lstat("/nonexistent"), std::system_error);
}

//--------------------------------------------------------------------------

TEST(CacheTree, MakeNode_Lstat)
{
    client::cache::Tree tree{};
    auto& root = tree.raw_get_root();
    auto& some = tree.raw_make_node(root, "some");
    EXPECT_NO_THROW(tree.lstat("/some"));
    tree.raw_make_node(root, "some2");
    EXPECT_NO_THROW(tree.lstat("/some2"));
    tree.raw_make_node(some, "sub");
    EXPECT_NO_THROW(tree.lstat("/some/sub"));
    EXPECT_THROW(tree.lstat("/some/sub2"), std::system_error);
}

//--------------------------------------------------------------------------

TEST(CacheTree, Chmod)
{
    client::cache::Tree tree{};
    auto& root = tree.raw_get_root();
    auto& some = tree.raw_make_node(root, "some");
    tree.raw_make_node(root, "some2");
    tree.raw_make_node(some, "sub");

    tree.chmod("/", 1);
    tree.chmod("/some", 20);
    tree.chmod("/some2", 333);
    tree.chmod("/some/sub", 4);
    EXPECT_THROW(tree.chmod("/some/sub2", 9), std::system_error);

    EXPECT_EQ(tree.lstat("/").st_mode, 1);
    EXPECT_EQ(tree.lstat("/some").st_mode, 20);
    EXPECT_EQ(tree.lstat("/some2").st_mode, 333);
    EXPECT_EQ(tree.lstat("/some/sub").st_mode, 4);
}

//==========================================================================
} // namespace rewofs::tests

