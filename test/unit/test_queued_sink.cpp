/**
 * @file test_queued_sink.cpp
 * @brief Unit tests for the asynchronous (queued) log sink decorator
 */

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "logging/log_sink.hpp"
#include "logging/mqtt_log_sink.hpp"
#include "logging/queued_sink.hpp"

using lopcore::LogLevel;
using lopcore::LogMessage;
using lopcore::MqttLogSink;
using lopcore::QueuedSink;
using lopcore::QueuedSinkConfig;

namespace
{

/**
 * @brief Inner sink that records every delivered message
 */
class RecordingSink final : public lopcore::ILogSink
{
public:
    void write(const LogMessage &msg) override
    {
        writes.push_back({msg.tag ? msg.tag : "", msg.message ? msg.message : ""});
    }
    void flush() override
    {
    }
    const char *getName() const override
    {
        return "RecordingSink";
    }

    struct Entry
    {
        std::string tag;
        std::string message;
    };
    std::vector<Entry> writes;
};

LogMessage makeMessage(const char *tag, const char *text, uint32_t ts = 0)
{
    LogMessage msg;
    msg.level = LogLevel::INFO;
    msg.timestamp_ms = ts;
    msg.tag = tag;
    msg.message = text;
    msg.file = nullptr;
    msg.line = 0;
    return msg;
}

} // namespace

TEST(QueuedSinkTest, WriteIsEnqueueOnly_PollDelivers)
{
    auto recording = std::make_unique<RecordingSink>();
    RecordingSink *inner = recording.get();
    QueuedSink sink(std::move(recording));

    sink.write(makeMessage("tag", "one"));
    sink.write(makeMessage("tag", "two"));
    EXPECT_TRUE(inner->writes.empty()); // nothing delivered synchronously

    EXPECT_EQ(sink.poll(), 2u);
    ASSERT_EQ(inner->writes.size(), 2u);
    EXPECT_EQ(inner->writes[0].message, "one"); // FIFO
    EXPECT_EQ(inner->writes[1].message, "two");

    EXPECT_EQ(sink.poll(), 0u); // queue drained
}

TEST(QueuedSinkTest, DeepCopy_SourceBufferMayBeReused)
{
    auto recording = std::make_unique<RecordingSink>();
    RecordingSink *inner = recording.get();
    QueuedSink sink(std::move(recording));

    // Logger reuses one stack buffer per log call; the sink must copy.
    char buffer[32];
    std::strcpy(buffer, "first");
    sink.write(makeMessage("tag", buffer));
    std::strcpy(buffer, "second");
    sink.write(makeMessage("tag", buffer));

    sink.poll();
    ASSERT_EQ(inner->writes.size(), 2u);
    EXPECT_EQ(inner->writes[0].message, "first");
    EXPECT_EQ(inner->writes[1].message, "second");
}

TEST(QueuedSinkTest, Overflow_DropsOldestAndCounts)
{
    QueuedSinkConfig config;
    config.maxQueuedMessages = 3;

    auto recording = std::make_unique<RecordingSink>();
    RecordingSink *inner = recording.get();
    QueuedSink sink(std::move(recording), config);

    for (int i = 0; i < 5; i++)
    {
        sink.write(makeMessage("tag", std::to_string(i).c_str()));
    }

    EXPECT_EQ(sink.droppedCount(), 2u);
    EXPECT_EQ(sink.poll(), 3u);
    ASSERT_EQ(inner->writes.size(), 3u);
    EXPECT_EQ(inner->writes[0].message, "2"); // oldest two dropped
    EXPECT_EQ(inner->writes[2].message, "4");
}

TEST(QueuedSinkTest, PollBatchLimitRespected)
{
    auto recording = std::make_unique<RecordingSink>();
    RecordingSink *inner = recording.get();
    QueuedSink sink(std::move(recording));

    for (int i = 0; i < 5; i++)
    {
        sink.write(makeMessage("tag", "m"));
    }

    EXPECT_EQ(sink.poll(2), 2u);
    EXPECT_EQ(inner->writes.size(), 2u);
    EXPECT_EQ(sink.poll(), 3u);
}

TEST(QueuedSinkTest, WriteDuringDrainIsDropped_NoFeedbackLoop)
{
    // Inner sink that logs back into the queued sink during delivery,
    // simulating the MQTT client's own log lines during publish.
    class FeedbackSink final : public lopcore::ILogSink
    {
    public:
        void write(const LogMessage &) override
        {
            delivered++;
            if (outer != nullptr)
            {
                LogMessage feedback = makeMessage("mqtt", "Published to ...");
                outer->write(feedback); // must be dropped, not re-enqueued
            }
        }
        void flush() override
        {
        }
        const char *getName() const override
        {
            return "FeedbackSink";
        }

        QueuedSink *outer = nullptr;
        int delivered = 0;
    };

    auto feedback = std::make_unique<FeedbackSink>();
    FeedbackSink *inner = feedback.get();
    QueuedSink sink(std::move(feedback));
    inner->outer = &sink;

    sink.write(makeMessage("app", "line"));
    EXPECT_EQ(sink.poll(), 1u);
    EXPECT_EQ(inner->delivered, 1);
    EXPECT_EQ(sink.droppedCount(), 1u); // the feedback write was dropped
    EXPECT_EQ(sink.poll(), 0u);         // and the queue stays empty: no loop
}

TEST(QueuedSinkTest, FlushIsANoOp)
{
    auto recording = std::make_unique<RecordingSink>();
    RecordingSink *inner = recording.get();
    QueuedSink sink(std::move(recording));

    sink.write(makeMessage("tag", "queued"));
    sink.flush();
    EXPECT_TRUE(inner->writes.empty()); // flush must not drain (Logger holds its mutex there)
}

TEST(MqttLogSinkTest, FormatsJsonAndPublishes)
{
    std::vector<std::string> published;
    MqttLogSink sink([&](const std::string &line) { published.push_back(line); });

    sink.write(makeMessage("MyTag", "hello world", 1234));

    ASSERT_EQ(published.size(), 1u);
    EXPECT_EQ(published[0], "{\"ts\":1234,\"lvl\":\"I\",\"tag\":\"MyTag\",\"msg\":\"hello world\"}");
}

TEST(MqttLogSinkTest, NullPublisherIsSafe)
{
    MqttLogSink sink(nullptr);
    sink.write(makeMessage("tag", "text")); // must not crash
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
