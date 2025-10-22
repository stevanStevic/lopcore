/**
 * @file console_sink.hpp
 * @brief Console output sink using ESP_LOG
 *
 * Week 2 - Logging System
 */

#pragma once

#include "log_sink.hpp"

namespace lopcore
{

/**
 * @brief Log sink that outputs to console via ESP_LOG
 *
 * Wraps ESP-IDF's ESP_LOG macros to integrate with LopCore logging.
 * Uses colored output on supported terminals.
 */
class ConsoleSink : public ILogSink
{
public:
    ConsoleSink();
    ~ConsoleSink() override = default;

    void write(const LogMessage &msg) override;
    void flush() override;
    const char *getName() const override;

    /**
     * @brief Enable/disable colored output
     * @param enable true to use colors
     */
    void setColorEnabled(bool enable)
    {
        use_colors_ = enable;
    }

private:
    bool use_colors_;
};

} // namespace lopcore
