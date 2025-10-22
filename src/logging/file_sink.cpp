/**
 * @file file_sink.cpp
 * @brief File logging sink implementation
 *
 * Week 2 - Logging System
 */

#include "lopcore/logging/file_sink.hpp"

#include <sys/stat.h>

#include <cstdio>
#include <cstring>

namespace lopcore
{

FileSink::FileSink(const FileSinkConfig &config)
    : config_(config), file_handle_(nullptr), bytes_written_(0), file_open_(false)
{
    buffer_.reserve(config_.buffer_size);
    openFile();
}

FileSink::~FileSink()
{
    flush();
    closeFile();
}

void FileSink::write(const LogMessage &msg)
{
    if (!file_open_)
    {
        return;
    }

    std::string formatted = formatMessage(msg);
    buffer_.append(formatted);

    // Flush if buffer is getting full
    if (buffer_.size() >= config_.buffer_size)
    {
        flush();
    }
}

void FileSink::flush()
{
    if (!file_open_ || buffer_.empty())
    {
        return;
    }

    FILE *fp = static_cast<FILE *>(file_handle_);
    size_t written = fwrite(buffer_.c_str(), 1, buffer_.size(), fp);
    fflush(fp);

    bytes_written_ += written;
    buffer_.clear();

    // Check if we need to rotate
    if (config_.auto_rotate)
    {
        checkRotation();
    }
}

const char *FileSink::getName() const
{
    return "FileSink";
}

size_t FileSink::getFileSize() const
{
    if (!file_open_)
    {
        return 0;
    }

    std::string path = getFilePath();
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
    {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
}

bool FileSink::isFileOpen() const
{
    return file_open_;
}

bool FileSink::rotate()
{
    flush();
    closeFile();

    // Delete old file
    std::string path = getFilePath();
    remove(path.c_str());

    // Reopen fresh file
    bytes_written_ = 0;
    return openFile();
}

std::string FileSink::getFilePath() const
{
    return config_.base_path + "/" + config_.filename;
}

bool FileSink::openFile()
{
    std::string path = getFilePath();

    file_handle_ = fopen(path.c_str(), "a");
    if (!file_handle_)
    {
        file_open_ = false;
        return false;
    }

    file_open_ = true;

    // Get current file size
    bytes_written_ = getFileSize();

    return true;
}

void FileSink::closeFile()
{
    if (file_handle_)
    {
        FILE *fp = static_cast<FILE *>(file_handle_);
        fclose(fp);
        file_handle_ = nullptr;
    }
    file_open_ = false;
}

void FileSink::checkRotation()
{
    if (bytes_written_ >= config_.max_file_size)
    {
        rotate();
    }
}

std::string FileSink::formatMessage(const LogMessage &msg) const
{
    char buffer[512];

    // Format: [timestamp] LEVEL (tag): message
    snprintf(buffer, sizeof(buffer), "[%10lu] %c (%s): %s\n", (unsigned long) msg.timestamp_ms,
             logLevelToChar(msg.level), msg.tag, msg.message);

    return std::string(buffer);
}

} // namespace lopcore
