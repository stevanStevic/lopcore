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
#include <cstring>

namespace lopcore
{

RingFileSink::RingFileSink(const RingFileSinkConfig &config) : config_(config), write_pos_(0)
{
    ensureFile();
    loadWritePos();
}

std::string RingFileSink::getFilePath() const
{
    return config_.base_path + "/" + config_.filename;
}

void RingFileSink::ensureFile()
{
    const std::string path = getFilePath();

    FILE *f = std::fopen(path.c_str(), "rb");
    if (f != nullptr)
    {
        std::fclose(f);
        return;
    }

    f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
    {
        return;
    }

    const uint64_t writePos = 0;
    std::fwrite(&writePos, sizeof(writePos), 1, f);

    // Pre-fill the body so the file is its final size up-front.
    const char zero = 0;
    for (size_t i = 0; i < config_.max_body_bytes; ++i)
    {
        std::fwrite(&zero, 1, 1, f);
    }
    std::fclose(f);
}

void RingFileSink::loadWritePos()
{
    const std::string path = getFilePath();
    FILE *f = std::fopen(path.c_str(), "rb");
    if (f == nullptr)
    {
        write_pos_ = 0;
        return;
    }
    if (std::fread(&write_pos_, sizeof(write_pos_), 1, f) != 1)
    {
        write_pos_ = 0;
    }
    std::fclose(f);
}

int RingFileSink::formatMessage(char *buf, size_t buf_size, const LogMessage &msg) const
{
    const char *tag = (msg.tag != nullptr) ? msg.tag : "";
    const char *txt = (msg.message != nullptr) ? msg.message : "";
    return std::snprintf(buf, buf_size, "[%10lu] %c (%s): %s\n", static_cast<unsigned long>(msg.timestamp_ms),
                         logLevelToChar(msg.level), tag, txt);
}

void RingFileSink::write(const LogMessage &msg)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.max_body_bytes == 0)
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

    const std::string path = getFilePath();
    FILE *f = std::fopen(path.c_str(), "r+b");
    if (f == nullptr)
    {
        return;
    }

    size_t remaining = bytes;
    const char *p = line;
    while (remaining > 0)
    {
        const size_t bodyOffset = static_cast<size_t>(write_pos_ % config_.max_body_bytes);
        const size_t spaceToEnd = config_.max_body_bytes - bodyOffset;
        const size_t chunk = (remaining < spaceToEnd) ? remaining : spaceToEnd;

        std::fseek(f, static_cast<long>(HEADER_SIZE + bodyOffset), SEEK_SET);
        std::fwrite(p, 1, chunk, f);

        p += chunk;
        remaining -= chunk;
        write_pos_ += chunk;
    }

    std::fseek(f, 0, SEEK_SET);
    std::fwrite(&write_pos_, sizeof(write_pos_), 1, f);
    std::fflush(f);
    std::fclose(f);
}

void RingFileSink::flush()
{
    // No buffering above the FILE* layer; writes already fflush'd.
}

const char *RingFileSink::getName() const
{
    return "RingFileSink";
}

uint64_t RingFileSink::getWritePos() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return write_pos_;
}

std::string RingFileSink::readAll() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string path = getFilePath();
    FILE *f = std::fopen(path.c_str(), "rb");
    if (f == nullptr)
    {
        return {};
    }

    uint64_t writePos = 0;
    std::fread(&writePos, sizeof(writePos), 1, f);

    const size_t maxBody = config_.max_body_bytes;
    const size_t total = (writePos < maxBody) ? static_cast<size_t>(writePos) : maxBody;
    if (total == 0)
    {
        std::fclose(f);
        return {};
    }

    std::string body;
    body.resize(total);

    if (writePos < maxBody)
    {
        std::fseek(f, static_cast<long>(HEADER_SIZE), SEEK_SET);
        std::fread(body.data(), 1, total, f);
    }
    else
    {
        // Wrapped. Oldest is at offset (writePos % maxBody); read tail then head.
        const size_t pos = static_cast<size_t>(writePos % maxBody);
        const size_t tailLen = maxBody - pos;
        std::fseek(f, static_cast<long>(HEADER_SIZE + pos), SEEK_SET);
        std::fread(body.data(), 1, tailLen, f);
        std::fseek(f, static_cast<long>(HEADER_SIZE), SEEK_SET);
        std::fread(body.data() + tailLen, 1, pos, f);
    }

    std::fclose(f);
    return body;
}

} // namespace lopcore
