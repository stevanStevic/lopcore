/**
 * @file ring_file_sink.cpp
 * @brief Fixed-size circular file log sink implementation
 *
 * @copyright Copyright (c) 2026 LopCore Contributors
 * @license MIT License
 */

#include "lopcore/logging/ring_file_sink.hpp"

#include <algorithm>
#include <cstdio>

namespace lopcore
{

RingFileSink::RingFileSink(const RingFileSinkConfig &config)
    : config_(config), storage_(config.base_path + "/" + config.filename, config.max_body_bytes)
{
}

std::string RingFileSink::getFilePath() const
{
    return storage_.path();
}

const char *RingFileSink::getName() const
{
    return "RingFileSink";
}

uint64_t RingFileSink::getWritePos() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return storage_.writePos();
}

int RingFileSink::formatMessage(char *buf, size_t buf_size, const LogMessage &msg) const
{
    const char *tag = (msg.tag != nullptr) ? msg.tag : "";
    const char *txt = (msg.message != nullptr) ? msg.message : "";
    return std::snprintf(buf, buf_size, "[%10lu] %c (%s): %s\n",
                         static_cast<unsigned long>(msg.timestamp_ms), logLevelToChar(msg.level), tag, txt);
}

void RingFileSink::write(const LogMessage &msg)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (storage_.maxBodyBytes() == 0)
    {
        return;
    }

    char line[512];
    const int n = formatMessage(line, sizeof(line), msg);
    if (n <= 0)
    {
        return;
    }
    const size_t bytes = static_cast<size_t>(std::min(static_cast<int>(sizeof(line) - 1), n));

    storage_.writeWrapped(storage_.writePos(), line, bytes);
    storage_.advanceAndPersist(bytes);
}

void RingFileSink::flush()
{
    // No buffering above the FILE* layer; writes are already fflush'd.
}

std::string RingFileSink::readAll() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    const uint64_t writePos = storage_.writePos();
    const size_t   maxBody  = storage_.maxBodyBytes();
    const size_t   total    = (writePos < maxBody) ? static_cast<size_t>(writePos) : maxBody;
    if (total == 0)
    {
        return {};
    }

    std::string body;
    body.resize(total);

    if (writePos < maxBody)
    {
        storage_.readBytes(0, body.data(), total);
    }
    else
    {
        const size_t pos     = static_cast<size_t>(writePos % maxBody);
        const size_t tailLen = maxBody - pos;
        storage_.readBytes(pos, body.data(), tailLen);
        storage_.readBytes(0, body.data() + tailLen, pos);
    }

    return body;
}

} // namespace lopcore
