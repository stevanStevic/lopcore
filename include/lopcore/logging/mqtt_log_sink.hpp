/**
 * @file mqtt_log_sink.hpp
 * @brief Log sink that publishes each log line as a small JSON MQTT message
 *
 * Header-only and client-agnostic: the sink takes a publish function instead
 * of a concrete MQTT client, so it works with CoreMqttClient, EspMqttClient
 * or any other transport, and stays host-testable.
 *
 * IMPORTANT: write() publishes synchronously. Never attach this sink to the
 * global Logger directly - Logger::logImpl() holds the Logger mutex across
 * sink->write(), and a synchronous publish from there can deadlock against
 * the MQTT client mutex (lock-order inversion between tasks). Always wrap it
 * in a QueuedSink (see queued_sink.hpp), which makes write() enqueue-only
 * and performs the publish from a safe drain context.
 *
 * @copyright Copyright (c) 2026 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <utility>

#include "log_level.hpp"
#include "log_sink.hpp"

namespace lopcore
{

/**
 * @brief Formats a LogMessage as one-line JSON and hands it to a publisher
 *
 * Usage (always behind a QueuedSink):
 * @code
 * auto sink = std::make_unique<QueuedSink>(std::make_unique<MqttLogSink>(
 *     [client, topic](const std::string &line) {
 *         client->publishString(topic, line, mqtt::MqttQos::AT_MOST_ONCE, false);
 *     }));
 * sink->startDrainTask();
 * Logger::getInstance().addSink(std::move(sink));
 * @endcode
 */
class MqttLogSink final : public ILogSink
{
public:
    using PublishFn = std::function<void(const std::string &payload)>;

    explicit MqttLogSink(PublishFn publish) : publish_(std::move(publish))
    {
    }

    ~MqttLogSink() override = default;

    MqttLogSink(const MqttLogSink &) = delete;
    MqttLogSink &operator=(const MqttLogSink &) = delete;
    MqttLogSink(MqttLogSink &&) = delete;
    MqttLogSink &operator=(MqttLogSink &&) = delete;

    void write(const LogMessage &msg) override
    {
        if (!publish_)
        {
            return;
        }

        // Note: msg text is not JSON-escaped; log lines containing '"' or
        // '\' produce technically invalid JSON. Consumers of the diag log
        // stream tolerate this; escaping every line was judged not worth the
        // per-message cost on-device.
        char buf[512];
        const char *tag = (msg.tag != nullptr) ? msg.tag : "";
        const char *txt = (msg.message != nullptr) ? msg.message : "";
        const int n = std::snprintf(buf, sizeof(buf),
                                    "{\"ts\":%lu,\"lvl\":\"%c\",\"tag\":\"%s\",\"msg\":\"%s\"}",
                                    static_cast<unsigned long>(msg.timestamp_ms), logLevelToChar(msg.level),
                                    tag, txt);
        if (n > 0)
        {
            publish_(std::string(buf, static_cast<size_t>(n) < sizeof(buf) ? static_cast<size_t>(n)
                                                                           : sizeof(buf) - 1));
        }
    }

    void flush() override
    {
        // No buffering above the publish layer
    }

    const char *getName() const override
    {
        return "MqttLogSink";
    }

private:
    PublishFn publish_; ///< Transport-agnostic publish hook (topic baked in by the caller)
};

} // namespace lopcore
