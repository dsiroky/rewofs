/// Tests entry.
///
/// @file

#include <string.h>

#include "rewofs/disablewarnings.hpp"
#include <gtest/gtest.h>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/log.hpp"

//==========================================================================

int main(int argc, char **argv)
{
    rewofs::log_init();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
