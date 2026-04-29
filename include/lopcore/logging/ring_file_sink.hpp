/**
 * @file ring_file_sink.hpp
 * @brief Fixed-size circular file log sink with persistent write position
 *
 * Unlike FileSink (which deletes the file when full), RingFileSink writes
 * to a fixed-size body and wraps in place when the body fills up. The most
 * recent N bytes of log output are always preserved. The current write
 * position is stored in a small file header so it survives reboots.
 *
 * Intended use: diagnostics / post-mortem console-log capture, where a
 * field operator reads the last N KB of logs after a fault. Not appropriate
 * for general-purpose log files where chronological completeness is needed.
 *
 * Internally backed by lopcore::detail::RingFileStorage; for typed binary
 * records use `lopcore::RingRecordFile<T>` instead.
 *
 * @copyright Copyright (c) 2026 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include "lopcore/storage/detail/ring_file_storage.hpp"

#include "log_sink.hpp"

namespace lopcore
{

/**
 * @brief Configuration for RingFileSink
 */
struct RingFileSinkConfig
{
    std::string base_path = "/spiffs";         ///< Mount point or directory
    std::string filename = "lopcore_ring.log"; ///< Log filename
    size_t max_body_bytes = 100 * 1024;        ///< Body size in bytes (file is HEADER+body)

    RingFileSinkConfig &setBasePath(const std::string &path)
    {
        base_path = path;
        return *this;
    }

    RingFileSinkConfig &setFilename(const std::string &name)
    {
        filename = name;
        return *this;
    }

    RingFileSinkConfig &setMaxBodyBytes(size_t bytes)
    {
        max_body_bytes = bytes;
        return *this;
    }
};

/**
 * @brief Log sink that writes to a fixed-size circular file
 *
 * Each `write()` formats the LogMessage into a text line and appends it
 * to the body, wrapping at the boundary if needed. Lines may split across
 * the wrap boundary - the byte stream stays usable because parsers can
 * resync on the next `\n`.
 */
class RingFileSink : public ILogSink
{
public:
    explicit RingFileSink(const RingFileSinkConfig &config = RingFileSinkConfig());

    ~RingFileSink() override = default;

    RingFileSink(const RingFileSink &) = delete;
    RingFileSink &operator=(const RingFileSink &) = delete;
    RingFileSink(RingFileSink &&) = delete;
    RingFileSink &operator=(RingFileSink &&) = delete;

    void write(const LogMessage &msg) override;
    void flush() override;
    const char *getName() const override;

    /**
     * @brief Read the entire body in chronological order (oldest first).
     */
    std::string readAll() const;

    /**
     * @brief Get the full path to the underlying file.
     */
    std::string getFilePath() const;

    /**
     * @brief Get the configured body size.
     */
    size_t getMaxBodyBytes() const
    {
        return config_.max_body_bytes;
    }

    /**
     * @brief Get the current monotonic write position (bytes ever written).
     *
     * @note Monotonically increases across the lifetime of the file and may
     *       exceed `max_body_bytes`. Take `% max_body_bytes` for the in-body
     *       offset.
     */
    uint64_t getWritePos() const;

private:
    RingFileSinkConfig      config_;
    detail::RingFileStorage storage_;
    mutable std::mutex      mutex_;

    int formatMessage(char *buf, size_t buf_size, const LogMessage &msg) const;
};

} // namespace lopcore
