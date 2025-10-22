/**
 * @file file_sink.hpp
 * @brief File-based log sink with ring buffer and rotation
 *
 * Logs to SPIFFS with automatic rotation when size limit reached.
 * Uses ring buffer approach for efficient storage.
 *
 * Week 2 - Logging System
 */

#pragma once

#include <memory>
#include <string>

#include "log_sink.hpp"

namespace lopcore
{

/**
 * @brief Configuration for file-based logging
 */
struct FileSinkConfig
{
    std::string base_path = "/spiffs";    ///< SPIFFS mount point
    std::string filename = "lopcore.log"; ///< Log filename
    size_t max_file_size = 100 * 1024;    ///< Max file size (100KB default)
    bool auto_rotate = true;              ///< Enable automatic rotation
    size_t buffer_size = 512;             ///< Write buffer size
};

/**
 * @brief Log sink that writes to SPIFFS file
 *
 * Features:
 * - Ring buffer rotation (overwrites oldest logs)
 * - Configurable size limit
 * - Buffered writes for efficiency
 * - Thread-safe operation
 * - Survives reboots
 *
 * @note Requires SPIFFS to be mounted before use
 *
 * @code
 * FileSinkConfig config;
 * config.max_file_size = 50 * 1024;  // 50KB
 * auto sink = std::make_unique<FileSink>(config);
 * logger.addSink(std::move(sink));
 * @endcode
 */
class FileSink : public ILogSink
{
public:
    /**
     * @brief Construct file sink with configuration
     * @param config File sink configuration
     */
    explicit FileSink(const FileSinkConfig &config = FileSinkConfig());

    /**
     * @brief Destructor - flushes and closes file
     */
    ~FileSink() override;

    void write(const LogMessage &msg) override;
    void flush() override;
    const char *getName() const override;

    /**
     * @brief Get current log file size
     * @return File size in bytes, or 0 if file not open
     */
    size_t getFileSize() const;

    /**
     * @brief Get configured maximum file size
     * @return Max size in bytes
     */
    size_t getMaxFileSize() const
    {
        return config_.max_file_size;
    }

    /**
     * @brief Check if file is currently open
     * @return true if file handle is valid
     */
    bool isFileOpen() const;

    /**
     * @brief Manually trigger log rotation
     * @return true if rotation succeeded
     */
    bool rotate();

    /**
     * @brief Get full path to log file
     * @return Complete file path
     */
    std::string getFilePath() const;

private:
    /**
     * @brief Open log file for appending
     * @return true if successfully opened
     */
    bool openFile();

    /**
     * @brief Close log file
     */
    void closeFile();

    /**
     * @brief Check if rotation is needed and perform it
     */
    void checkRotation();

    /**
     * @brief Format log message for file output
     * @param msg Log message to format
     * @return Formatted string
     */
    std::string formatMessage(const LogMessage &msg) const;

    FileSinkConfig config_; ///< Configuration
    void *file_handle_;     ///< FILE* handle (void* for portability)
    std::string buffer_;    ///< Write buffer
    size_t bytes_written_;  ///< Bytes written since last rotation
    bool file_open_;        ///< File state flag
};

} // namespace lopcore
