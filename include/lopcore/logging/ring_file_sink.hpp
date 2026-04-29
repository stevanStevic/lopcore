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
 * @copyright Copyright (c) 2026 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include "log_sink.hpp"

namespace lopcore
{

/**
 * @brief Configuration for RingFileSink
 *
 * @code
 * RingFileSinkConfig config;
 * config.setBasePath("/diag")
 *       .setFilename("console.log")
 *       .setMaxBodyBytes(256 * 1024);
 *
 * auto sink = std::make_unique<RingFileSink>(config);
 * logger.addSink(std::move(sink));
 * @endcode
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
 * File layout:
 *   bytes [0, HEADER_SIZE):           u64 writePos (monotonically increasing)
 *   bytes [HEADER_SIZE, HEADER_SIZE+max_body_bytes): circular body
 *
 * On each write, formatted bytes are placed at offset
 * `HEADER_SIZE + (writePos % max_body_bytes)`, wrapping back to the start
 * of the body when needed. After every write the new writePos is persisted
 * to bytes [0, 8) so the position survives reboots.
 *
 * Thread safety: the public methods serialize on an internal mutex so this
 * sink is safe to share with concurrent `write()` and `readAll()` callers.
 *
 * @note The file size is fixed at construction time. Changing `max_body_bytes`
 *       between runs while pointing at the same file leads to undefined data
 *       (the wrap arithmetic changes meaning). Use a different filename or
 *       erase the file when changing size.
 */
class RingFileSink : public ILogSink
{
public:
    /**
     * @brief Construct a ring file sink
     * @param config Configuration; base_path/filename combined to form the path
     */
    explicit RingFileSink(const RingFileSinkConfig &config = RingFileSinkConfig());

    /**
     * @brief Destructor
     */
    ~RingFileSink() override = default;

    RingFileSink(const RingFileSink &) = delete;
    RingFileSink &operator=(const RingFileSink &) = delete;
    RingFileSink(RingFileSink &&) = delete;
    RingFileSink &operator=(RingFileSink &&) = delete;

    void write(const LogMessage &msg) override;
    void flush() override;
    const char *getName() const override;

    /**
     * @brief Read the entire body in chronological order (oldest first)
     *
     * If the file has not yet wrapped, returns bytes [HEADER, HEADER+writePos).
     * If wrapped, stitches the tail (oldest) followed by the head (newer).
     *
     * @return Body bytes as a string (binary-safe)
     */
    std::string readAll() const;

    /**
     * @brief Get the full path to the underlying file
     */
    std::string getFilePath() const;

    /**
     * @brief Get the configured body size
     */
    size_t getMaxBodyBytes() const
    {
        return config_.max_body_bytes;
    }

    /**
     * @brief Get the current monotonic write position (bytes ever written)
     *
     * @note This value monotonically increases and may exceed `max_body_bytes`.
     *       Take `% max_body_bytes` to get the in-body offset.
     */
    uint64_t getWritePos() const;

private:
    static constexpr size_t HEADER_SIZE = 8; ///< u64 writePos

    RingFileSinkConfig config_;
    uint64_t write_pos_;
    mutable std::mutex mutex_;

    /// Create the file and pre-fill it if it does not yet exist.
    void ensureFile();

    /// Read writePos from disk into write_pos_.
    void loadWritePos();

    /// Format a LogMessage as text into `buf`. Returns bytes written (excluding NUL).
    int formatMessage(char *buf, size_t buf_size, const LogMessage &msg) const;
};

} // namespace lopcore
