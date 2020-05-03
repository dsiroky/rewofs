/// Test watcher logic.
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <gtest/gtest.h>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/server/watcher.hpp"

//==========================================================================
namespace rewofs::tests {
//==========================================================================

static constexpr std::chrono::steady_clock::time_point NOW{};

//==========================================================================

TEST(TemporalIgnores, Empty)
{
    using namespace std::chrono_literals;

    server::TemporalIgnores ignores{1s};

    EXPECT_FALSE(ignores.check(NOW, "/a"));
    EXPECT_FALSE(ignores.check(NOW, "/b"));
    EXPECT_FALSE(ignores.check(NOW + 999ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 1100ms, "/a"));
}

//--------------------------------------------------------------------------

TEST(TemporalIgnores, SingleAdd)
{
    using namespace std::chrono_literals;

    server::TemporalIgnores ignores{1s};

    ignores.add(NOW, "/a");

    EXPECT_TRUE(ignores.check(NOW, "/a"));
    EXPECT_FALSE(ignores.check(NOW, "/b"));
    EXPECT_TRUE(ignores.check(NOW + 999ms, "/a"));
    EXPECT_TRUE(ignores.check(NOW + 1000ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 1001ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 1100ms, "/a"));
}

//--------------------------------------------------------------------------

TEST(TemporalIgnores, MultipleAdds)
{
    using namespace std::chrono_literals;

    server::TemporalIgnores ignores{1s};

    ignores.add(NOW, "/a");
    ignores.add(NOW + 500ms, "/b");

    EXPECT_TRUE(ignores.check(NOW + 500ms, "/a"));
    EXPECT_TRUE(ignores.check(NOW + 500ms, "/b"));
    EXPECT_TRUE(ignores.check(NOW + 999ms, "/a"));
    EXPECT_TRUE(ignores.check(NOW + 999ms, "/b"));
    EXPECT_FALSE(ignores.check(NOW + 1100ms, "/a"));
    EXPECT_TRUE(ignores.check(NOW + 1100ms, "/b"));
    EXPECT_FALSE(ignores.check(NOW + 1600ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 1600ms, "/b"));

    ignores.add(NOW + 5s, "/c");
    EXPECT_FALSE(ignores.check(NOW + 5000ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 5000ms, "/b"));
    EXPECT_TRUE(ignores.check(NOW + 5000ms, "/c"));
    EXPECT_TRUE(ignores.check(NOW + 5500ms, "/c"));
    EXPECT_FALSE(ignores.check(NOW + 6100ms, "/c"));
}

//--------------------------------------------------------------------------

TEST(TemporalIgnores, MultipleAddsOfTheSameBeforeExpiring)
{
    using namespace std::chrono_literals;

    server::TemporalIgnores ignores{1s};

    ignores.add(NOW, "/a");
    ignores.add(NOW + 500ms, "/b");
    ignores.add(NOW + 700ms, "/a");

    EXPECT_TRUE(ignores.check(NOW + 700ms, "/a"));
    EXPECT_TRUE(ignores.check(NOW + 700ms, "/b"));
    EXPECT_TRUE(ignores.check(NOW + 1100ms, "/a"));
    EXPECT_TRUE(ignores.check(NOW + 1100ms, "/b"));
    EXPECT_TRUE(ignores.check(NOW + 1600ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 1600ms, "/b"));
}

//--------------------------------------------------------------------------

TEST(TemporalIgnores, MultipleAddsOfTheSameAfterExpiring)
{
    using namespace std::chrono_literals;

    server::TemporalIgnores ignores{1s};

    ignores.add(NOW, "/a");
    ignores.add(NOW + 500ms, "/b");
    ignores.add(NOW + 2000ms, "/a");

    EXPECT_TRUE(ignores.check(NOW + 2000ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 2000ms, "/b"));
    EXPECT_TRUE(ignores.check(NOW + 2900ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 2900ms, "/b"));
    EXPECT_FALSE(ignores.check(NOW + 3100ms, "/a"));
    EXPECT_FALSE(ignores.check(NOW + 3100ms, "/b"));
}

//==========================================================================
} // namespace rewofs::tests
