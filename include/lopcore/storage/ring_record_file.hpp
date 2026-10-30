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

#include <algorithm>
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
     * @brief Records kept from sequence number `firstSeq` on, oldest-first.
     *
     * A record's sequence number is its 0-based position among every record
     * appended since the last clear(), overwritten ones included.
     */
    struct Slice
    {
        uint64_t       firstSeq;
        std::vector<T> records;
    };

    /**
     * @brief Records with sequence number `seq` or later, in one locked read.
     *
     * Compare `firstSeq` with `seq`: equal when nothing was lost, greater when
     * the ring overwrote `firstSeq - seq` of them first, smaller when the log
     * holds fewer than `seq` records (cleared or recreated since `seq` was
     * taken). A caller that tracks what it sent keeps `firstSeq +
     * records.size()` as its next `seq`.
     *
     * Sequence numbers restart at 0 after clear() or when the file is
     * recreated, so `firstSeq < seq` only detects a restart while fewer than
     * `seq` records were appended since. A caller whose cursor must survive
     * either has to reset it on that event itself.
     */
    Slice readFrom(uint64_t seq) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        const uint64_t total  = storage_.writePos() / sizeof(T);
        const uint64_t oldest = (total > max_entries_) ? total - max_entries_ : 0u;

        Slice out{seq < oldest ? oldest : (seq > total ? total : seq), {}};
        const size_t count = static_cast<size_t>(total - out.firstSeq);
        if (count == 0)
        {
            return out;
        }

        // The kept records are contiguous in the body except across the wrap,
        // so at most two reads (each one opens the file).
        out.records.resize(count);
        const size_t startSlot = static_cast<size_t>(out.firstSeq % max_entries_);
        const size_t firstRun  = std::min(count, static_cast<size_t>(max_entries_) - startSlot);
        storage_.readBytes(startSlot * sizeof(T), out.records.data(), firstRun * sizeof(T));
        if (firstRun < count)
        {
            storage_.readBytes(0, out.records.data() + firstRun, (count - firstRun) * sizeof(T));
        }
        return out;
    }

    /**
     * @brief Return all valid records oldest-first.
     */
    std::vector<T> readAll() const
    {
        return readFrom(0).records;
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
