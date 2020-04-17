/// Test caches.
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/client/cache.hpp"

//==========================================================================
namespace rewofs::tests {
//==========================================================================

namespace t = testing;

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

TEST(Content, RW)
{
    client::cache::Content content{};

    EXPECT_FALSE(content.read("/abc", 10, 50, {}));

    content.write("/d", 10, {1, 2, 3});

    EXPECT_FALSE(content.read("/d", 0, 2, {}));
    EXPECT_FALSE(content.read("/d", 10, 2, {}));
    EXPECT_FALSE(content.read("/d", 10, 50, {}));

    {
        std::vector<uint8_t> out{};
        EXPECT_TRUE(content.read("/d", 10, 3, [&out](const auto& buf) {
            std::copy(buf.begin(), buf.end(), std::back_inserter(out));
        }));
        EXPECT_THAT(out, t::ElementsAre(1, 2, 3));
    }

    content.write("/a", 20, {4, 5, 6});

    EXPECT_FALSE(content.read("/d", 20, 3, {}));

    {
        std::vector<uint8_t> out{};
        EXPECT_TRUE(content.read("/d", 10, 3, [&out](const auto& buf) {
            std::copy(buf.begin(), buf.end(), std::back_inserter(out));
        }));
        EXPECT_THAT(out, t::ElementsAre(1, 2, 3));
    }
    {
        std::vector<uint8_t> out{};
        EXPECT_TRUE(content.read("/a", 20, 3, [&out](const auto& buf) {
            std::copy(buf.begin(), buf.end(), std::back_inserter(out));
        }));
        EXPECT_THAT(out, t::ElementsAre(4, 5, 6));
    }

    content.write("/d", 11, {40, 41});

    {
        std::vector<uint8_t> out{};
        EXPECT_TRUE(content.read("/d", 10, 3, [&out](const auto& buf) {
            std::copy(buf.begin(), buf.end(), std::back_inserter(out));
        }));
        EXPECT_THAT(out, t::ElementsAre(1, 40, 41));
    }
}

//--------------------------------------------------------------------------

TEST(Content, DeletePath)
{
    client::cache::Content content{};

    content.write("/a", 10, {1, 2, 3});
    content.write("/b", 10, {1, 2, 3});
    content.write("/c", 10, {1, 2, 3});
    content.write("/b", 20, {1, 2, 3});

    EXPECT_TRUE(content.read("/a", 10, 3, [](const auto&){}));
    EXPECT_TRUE(content.read("/c", 10, 3, [](const auto&){}));
    EXPECT_TRUE(content.read("/b", 10, 3, [](const auto&){}));
    EXPECT_TRUE(content.read("/b", 20, 3, [](const auto&){}));

    content.delete_file("/b");

    EXPECT_TRUE(content.read("/a", 10, 3, [](const auto&){}));
    EXPECT_TRUE(content.read("/c", 10, 3, [](const auto&){}));
    EXPECT_FALSE(content.read("/b", 10, 3, [](const auto&){}));
    EXPECT_FALSE(content.read("/b", 20, 3, [](const auto&){}));
}

//==========================================================================
} // namespace rewofs::tests
