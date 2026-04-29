/**
 * @file ring_record_file.hpp
 * @brief Fixed-capacity circular file of typed binary records
 *
 * Stores at most `max_entries` records of type `T` on disk. Each record
 * occupies a whole slot, so records never split across the wrap boundary.
 * The file header carries a monotonic byte writePos; per-slot index and
 * count are derived: `count = min(max_entries, writePos / sizeof(T))`,
 * `slot = (writePos / sizeof(T)) % max_entries`.
 *
 * Companion to RingFileSink (which is byte-oriented for log lines).
 * Internally backed by lopcore::detail::RingFileStorage.
 *
 * @copyright Copyright (c) 2026 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "lopcore/storage/detail/ring_file_storage.hpp"

namespace lopcore
{

/**
 * @brief Fixed-capacity circular file of typed binary records
 *
 * @tparam T trivially-copyable record type with a stable in-memory layout.
 */
template <typename T>
class RingRecordFile
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "RingRecordFile<T>: T must be trivially copyable for binary I/O");

public:
    RingRecordFile(std::string path, uint32_t max_entries)
        : storage_(std::move(path), static_cast<size_t>(max_entries) * sizeof(T)),
          max_entries_(max_entries)
    {
    }

    ~RingRecordFile() = default;

    RingRecordFile(const RingRecordFile &) = delete;
    RingRecordFile &operator=(const RingRecordFile &) = delete;
    RingRecordFile(RingRecordFile &&) = delete;
    RingRecordFile &operator=(RingRecordFile &&) = delete;

    /**
     * @brief Append a record. Overwrites the oldest entry on overflow.
     */
    void append(const T &entry)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (max_entries_ == 0)
        {
            return;
        }

        const uint64_t totalRecords = storage_.writePos() / sizeof(T);
        const uint32_t slot         = static_cast<uint32_t>(totalRecords % max_entries_);
        const size_t   bodyOffset   = static_cast<size_t>(slot) * sizeof(T);

        storage_.writeWrapped(bodyOffset, &entry, sizeof(T));
        storage_.advanceAndPersist(sizeof(T));
    }

    /**
     * @brief Return all valid records oldest-first.
     */
    std::vector<T> readAll() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        const uint64_t totalRecords = storage_.writePos() / sizeof(T);
        const uint32_t count =
            (totalRecords < max_entries_) ? static_cast<uint32_t>(totalRecords) : max_entries_;
        if (count == 0)
        {
            return {};
        }

        std::vector<T> out;
        out.reserve(count);

        const uint32_t startSlot =
            (totalRecords < max_entries_) ? 0u : static_cast<uint32_t>(totalRecords % max_entries_);

        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t slot       = (startSlot + i) % max_entries_;
            const size_t   bodyOffset = static_cast<size_t>(slot) * sizeof(T);
            T              e{};
            storage_.readBytes(bodyOffset, &e, sizeof(T));
            out.push_back(e);
        }
        return out;
    }

    /**
     * @brief Reset the log to empty.
     */
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_.resetAndPersist();
    }

    /**
     * @brief Configured slot capacity.
     */
    uint32_t maxEntries() const
    {
        return max_entries_;
    }

private:
    detail::RingFileStorage storage_;
    uint32_t                max_entries_;
    mutable std::mutex      mutex_;
};

} // namespace lopcore
