/**
 * @file ring_file_storage.cpp
 * @brief Internal storage primitive for fixed-size circular files
 *
 * @copyright Copyright (c) 2026 LopCore Contributors
 * @license MIT License
 */

#include "lopcore/storage/detail/ring_file_storage.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

namespace lopcore::detail
{

RingFileStorage::RingFileStorage(std::string path, size_t max_body_bytes)
    : path_(std::move(path)), max_body_bytes_(max_body_bytes), write_pos_(0)
{
    ensureFile();
    loadWritePos();
}

void RingFileStorage::ensureFile()
{
    FILE *f = std::fopen(path_.c_str(), "rb");
    if (f != nullptr)
    {
        std::fclose(f);
        return;
    }

    f = std::fopen(path_.c_str(), "wb");
    if (f == nullptr)
    {
        return;
    }

    const uint64_t writePos = 0;
    std::fwrite(&writePos, sizeof(writePos), 1, f);

    const char zero = 0;
    for (size_t i = 0; i < max_body_bytes_; ++i)
    {
        std::fwrite(&zero, 1, 1, f);
    }
    std::fclose(f);
}

void RingFileStorage::loadWritePos()
{
    FILE *f = std::fopen(path_.c_str(), "rb");
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

void RingFileStorage::persistWritePos()
{
    FILE *f = std::fopen(path_.c_str(), "r+b");
    if (f == nullptr)
    {
        return;
    }
    std::fseek(f, 0, SEEK_SET);
    std::fwrite(&write_pos_, sizeof(write_pos_), 1, f);
    std::fflush(f);
    std::fclose(f);
}

void RingFileStorage::writeWrapped(uint64_t pos, const void *data, size_t len)
{
    if (max_body_bytes_ == 0 || len == 0 || data == nullptr)
    {
        return;
    }

    FILE *f = std::fopen(path_.c_str(), "r+b");
    if (f == nullptr)
    {
        return;
    }

    const auto *p         = static_cast<const unsigned char *>(data);
    size_t      remaining = len;
    uint64_t    curPos    = pos;

    while (remaining > 0)
    {
        const size_t bodyOffset = static_cast<size_t>(curPos % max_body_bytes_);
        const size_t spaceToEnd = max_body_bytes_ - bodyOffset;
        const size_t chunk      = (remaining < spaceToEnd) ? remaining : spaceToEnd;

        std::fseek(f, static_cast<long>(HEADER_SIZE + bodyOffset), SEEK_SET);
        std::fwrite(p, 1, chunk, f);

        p += chunk;
        remaining -= chunk;
        curPos += chunk;
    }

    std::fflush(f);
    std::fclose(f);
}

void RingFileStorage::readBytes(size_t body_offset, void *out, size_t len) const
{
    if (out == nullptr || len == 0 || body_offset >= max_body_bytes_)
    {
        return;
    }
    FILE *f = std::fopen(path_.c_str(), "rb");
    if (f == nullptr)
    {
        return;
    }
    std::fseek(f, static_cast<long>(HEADER_SIZE + body_offset), SEEK_SET);
    std::fread(out, 1, len, f);
    std::fclose(f);
}

void RingFileStorage::advanceAndPersist(uint64_t bytes)
{
    write_pos_ += bytes;
    persistWritePos();
}

void RingFileStorage::resetAndPersist()
{
    write_pos_ = 0;
    persistWritePos();
}

} // namespace lopcore::detail
