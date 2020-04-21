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

//--------------------------------------------------------------------------

TEST(CacheTree, Exchange)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    auto& node_a = tree.make_node(root, "node_a");
    node_a.st.st_size = 100;
    auto& node_b = tree.make_node(root, "node_b");
    node_b.st.st_size = 1000;

    tree.exchange("/node_a", "/node_b");

    EXPECT_EQ(tree.get_node("/node_a").st.st_size, 1000);
    EXPECT_EQ(tree.get_node("/node_b").st.st_size, 100);
}

//--------------------------------------------------------------------------

TEST(CacheTree, ExchangeRoot)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    tree.make_node(root, "node_a");

    EXPECT_THROW(tree.exchange("/", "/node_a"), std::exception);
    EXPECT_THROW(tree.exchange("/node_a", "/"), std::exception);
}

//--------------------------------------------------------------------------

TEST(CacheTree, ExchangeTheSame)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    tree.make_node(root, "node_a");

    EXPECT_THROW(tree.exchange("/node_a", "/node_a"), std::exception);
}

//--------------------------------------------------------------------------

TEST(CacheTree, ExchangeDifferentDirectory)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    auto& node_a = tree.make_node(root, "node_a");
    node_a.st.st_size = 100;
    auto& subdir = tree.make_node(root, "subdir");
    auto& node_b = tree.make_node(subdir, "node_b");
    node_b.st.st_size = 1000;

    tree.exchange("/node_a", "/subdir/node_b");

    EXPECT_EQ(tree.get_node("/node_a").st.st_size, 1000);
    EXPECT_EQ(tree.get_node("/subdir/node_b").st.st_size, 100);
}

//--------------------------------------------------------------------------

TEST(CacheTree, ExchangeDirectories)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    auto& subdir_a = tree.make_node(root, "sub_a");
    auto& node_a = tree.make_node(subdir_a, "node_a");
    node_a.st.st_size = 100;
    auto& subdir_b = tree.make_node(root, "sub_b");
    auto& node_b = tree.make_node(subdir_b, "node_b");
    node_b.st.st_size = 1000;

    tree.exchange("/sub_a", "/sub_b");

    EXPECT_EQ(tree.get_node("/sub_a/node_b").st.st_size, 1000);
    EXPECT_EQ(tree.get_node("/sub_b/node_a").st.st_size, 100);
    EXPECT_THROW(tree.get_node("/sub_a/node_a"), std::exception);
    EXPECT_THROW(tree.get_node("/sub_b/node_b"), std::exception);
}

//--------------------------------------------------------------------------

TEST(CacheTree, ExchangeMissing)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    tree.make_node(root, "node_a");

    EXPECT_THROW(tree.exchange("/nonexistent1", "/nonexistent2"), std::exception);
    EXPECT_THROW(tree.exchange("/node_a", "/nonexistent"), std::exception);
    EXPECT_THROW(tree.exchange("/nonexistent", "/node_a"), std::exception);
}

//--------------------------------------------------------------------------

TEST(CacheTree, ExchangeSubDirectory)
{
    client::cache::Tree tree{};
    auto& root = tree.get_root();
    auto& s1 = tree.make_node(root, "s1");
    tree.make_node(s1, "s2");

    EXPECT_THROW(tree.exchange("/", "/s1/s2"), std::exception);
    EXPECT_THROW(tree.exchange("/s1/s2", "/"), std::exception);
    EXPECT_THROW(tree.exchange("/s1", "/s1/s2"), std::exception);
    EXPECT_THROW(tree.exchange("/s1/s2", "/s1"), std::exception);
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
