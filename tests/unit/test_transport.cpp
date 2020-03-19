/// Test common transport logic.
///
/// @file

#include "rewofs/disablewarnings.hpp"
#include <gtest/gtest.h>
#include <flatbuffers/flatbuffers.h>
#include "rewofs/messages/all.hpp"
#include "rewofs/enablewarnings.hpp"

#include "rewofs/transport.hpp"

//==========================================================================
namespace rewofs::tests {
//==========================================================================

namespace rmsg = rewofs::messages;

//==========================================================================

TEST(Serializer, AddCommand_SingleQueue)
{
    Serializer serializer{345};

    auto queue = serializer.new_queue(Serializer::Priority{7});

    {
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_open(fbb, "/some/file", 3);
        serializer.add_command(queue, fbb, command);
    }
    {
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_close(fbb, "/other/iii");
        serializer.add_command(queue, fbb, command);
    }

    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 345);
        ASSERT_EQ(frame->message_type(), rmsg::Message::CommandOpen);
        const auto message = static_cast<const rmsg::CommandOpen*>(frame->message());
        EXPECT_EQ(message->path()->str(), "/some/file");
    }
    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 346);
        ASSERT_EQ(frame->message_type(), rmsg::Message::CommandClose);
    }
    { // no more messages
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_EQ(raw_frame.size(), 0);
    }
}

//--------------------------------------------------------------------------

TEST(Serializer, AddCommand_MultipleQueues_Priorities)
{
    Serializer serializer{345};

    auto queue1 = serializer.new_queue(Serializer::Priority{100});
    auto queue2 = serializer.new_queue(Serializer::Priority{0});
    auto queue3 = serializer.new_queue(Serializer::Priority{100});

    {
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_open(fbb, "/a", 3);
        serializer.add_command(queue1, fbb, command);
    }
    {
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_open(fbb, "/b", 3);
        serializer.add_command(queue2, fbb, command);
    }
    {
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_open(fbb, "/c", 3);
        serializer.add_command(queue3, fbb, command);
    }

    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 345);
        const auto cmd = static_cast<const rmsg::CommandOpen*>(frame->message());
        EXPECT_EQ(cmd->path()->str(), "/a");
    }
    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 347);
        const auto cmd = static_cast<const rmsg::CommandOpen*>(frame->message());
        EXPECT_EQ(cmd->path()->str(), "/c");
    }
    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 346);
        const auto cmd = static_cast<const rmsg::CommandOpen*>(frame->message());
        EXPECT_EQ(cmd->path()->str(), "/b");
    }
    { // no more messages
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_EQ(raw_frame.size(), 0);
    }
}

//--------------------------------------------------------------------------

TEST(Serializer, AddCommand_MultipleInterleavedQueues_Priorities)
{
    Serializer serializer{345};

    auto queue1 = serializer.new_queue(Serializer::Priority{100});
    auto queue2 = serializer.new_queue(Serializer::Priority{0});

    {
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_open(fbb, "/a", 3);
        serializer.add_command(queue1, fbb, command);
    }
    {
        auto queue = serializer.new_queue(Serializer::Priority{0});
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_open(fbb, "/b", 3);
        serializer.add_command(queue2, fbb, command);
    }
    {
        auto queue = serializer.new_queue(Serializer::Priority{100});
        flatbuffers::FlatBufferBuilder fbb{};
        auto command = make_command_open(fbb, "/c", 3);
        serializer.add_command(queue1, fbb, command);
    }

    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 345);
        const auto cmd = static_cast<const rmsg::CommandOpen*>(frame->message());
        EXPECT_EQ(cmd->path()->str(), "/a");
    }
    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 347);
        const auto cmd = static_cast<const rmsg::CommandOpen*>(frame->message());
        EXPECT_EQ(cmd->path()->str(), "/c");
    }
    {
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_GT(raw_frame.size(), 0);
        const auto frame = rmsg::GetFrame(raw_frame.data());
        EXPECT_EQ(frame->id(), 346);
        const auto cmd = static_cast<const rmsg::CommandOpen*>(frame->message());
        EXPECT_EQ(cmd->path()->str(), "/b");
    }
    { // no more messages
        std::vector<uint8_t> raw_frame{};
        serializer.pop([&raw_frame](const gsl::span<const uint8_t> buf) {
            raw_frame.assign(buf.data(), buf.data() + buf.size());
        });
        ASSERT_EQ(raw_frame.size(), 0);
    }
}

//==========================================================================

TEST(Distributor, ProcessEmptyMessage_NothingHappens)
{
    bool open_called{false};

    rewofs::Distributor distributor {};
    distributor.subscribe<rmsg::CommandOpen>(
        [&open_called](const MessageId, const rmsg::CommandOpen&) {
            open_called = true;
        });

    distributor.process_frame({});

    EXPECT_FALSE(open_called);
}

//--------------------------------------------------------------------------

TEST(Distributor, ProcessBrokenMessage_NothingHappens)
{
    bool open_called{false};

    rewofs::Distributor distributor {};
    distributor.subscribe<rmsg::CommandOpen>(
        [&open_called](const MessageId, const rmsg::CommandOpen&) {
            open_called = true;
        });

    distributor.process_frame({reinterpret_cast<const uint8_t*>("aaaaaaaaaaaaaaaa"), 16});

    EXPECT_FALSE(open_called);
}

//--------------------------------------------------------------------------

TEST(Distributor, ProcessKnownMessage_CallsSubscribedCallback)
{
    bool open_called{false};
    bool close_called{false};

    rewofs::Distributor distributor{};
    distributor.subscribe<rmsg::CommandOpen>(
        [&open_called](const MessageId mid, const rmsg::CommandOpen& msg) {
            open_called = true;
            EXPECT_EQ(mid, MessageId{6442});
            EXPECT_EQ(msg.path()->str(), std::string{"/this/file"});
            EXPECT_EQ(msg.flags(), 98);
            EXPECT_FALSE(flatbuffers::IsFieldPresent(&msg, rmsg::CommandOpen::VT_MODE));
        });
    distributor.subscribe<rmsg::CommandClose>(
        [&close_called](const MessageId, const rmsg::CommandClose&) {
            close_called = true;
        });

    flatbuffers::FlatBufferBuilder fbb{};
    auto command = make_command_open(fbb, "/this/file", 98);
    auto frame = make_frame(fbb, 6442, command);
    fbb.Finish(frame);

    distributor.process_frame({fbb.GetBufferPointer(), static_cast<size_t>(fbb.GetSize())});

    EXPECT_TRUE(open_called);
    EXPECT_FALSE(close_called);
}

//==========================================================================

TEST(Deserializer, ProcessEmptyMessage_NothingHappens)
{
    Deserializer deserializer{};

    deserializer.process_frame({});

    EXPECT_FALSE(deserializer.wait_for_result<rmsg::ResultRead>(
        MessageId{4}, std::chrono::milliseconds{1}).is_valid());
}

//--------------------------------------------------------------------------

TEST(Deserializer, ProcessBrokenMessage_NothingHappens)
{
    Deserializer deserializer{};

    deserializer.process_frame({reinterpret_cast<const uint8_t*>("aaaaaaaaaaaaaaaa"), 16});

    EXPECT_FALSE(deserializer.wait_for_result<rmsg::ResultRead>(
        MessageId{4}, std::chrono::milliseconds{1}).is_valid());
}

//--------------------------------------------------------------------------

TEST(Deserializer, ProcessUnwantedMessageWithSameId_NotConsumed)
{
    Deserializer deserializer{};

    flatbuffers::FlatBufferBuilder fbb{};
    const auto cmd = messages::CreateResultErrno(fbb, 333);
    const auto frame = make_frame(fbb, 4, cmd);
    fbb.Finish(frame);

    deserializer.process_frame(
        {fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize()});

    EXPECT_FALSE(deserializer.wait_for_result<rmsg::ResultRead>(
        MessageId{4}, std::chrono::milliseconds{1}).is_valid());
}

//--------------------------------------------------------------------------

TEST(Deserializer, ProcessWantedMessageWithDifferentId_NotConsume)
{
    Deserializer deserializer{};

    flatbuffers::FlatBufferBuilder fbb{};
    const auto cmd = messages::CreateResultErrno(fbb, 333);
    const auto frame = make_frame(fbb, 4, cmd);
    fbb.Finish(frame);

    deserializer.process_frame(
        {fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize()});

    EXPECT_FALSE(deserializer.wait_for_result<rmsg::ResultErrno>(
        MessageId{5}, std::chrono::milliseconds{1}).is_valid());
}

//--------------------------------------------------------------------------

TEST(Deserializer, ProcessWantedMessage_ConsumeAfterProcess)
{
    Deserializer deserializer{};

    flatbuffers::FlatBufferBuilder fbb{};
    const auto cmd = messages::CreateResultErrno(fbb, 333);
    const auto frame = make_frame(fbb, 4, cmd);
    fbb.Finish(frame);

    deserializer.process_frame(
        {fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize()});

    {
        const auto result = deserializer.wait_for_result<rmsg::ResultErrno>(
            MessageId{4}, std::chrono::milliseconds{1});
        EXPECT_TRUE(result.is_valid());
        EXPECT_EQ(result.message().res_errno(), 333);
    }
    { // consumed
        EXPECT_FALSE(deserializer.wait_for_result<rmsg::ResultErrno>(
            MessageId{4}, std::chrono::milliseconds{1}).is_valid());
    }
}

//==========================================================================
} // namespace rewofs::tests
