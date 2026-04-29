/**
 * @file ring_file_storage.hpp
 * @brief Internal storage primitive for fixed-size circular files
 *
 * @warning Implementation detail. Do not use directly. Use one of the public
 *          view classes:
 *            - lopcore::RingFileSink     (byte stream, ILogSink)
 *            - lopcore::RingRecordFile<T> (typed records)
 *
 * Owns a fixed-size file consisting of:
 *   bytes [0, 8):                  u64 writePos (monotonic byte counter)
 *   bytes [8, 8 + max_body_bytes): circular body
 *
 * Provides wrap-aware writes that split across the body boundary if a
 * single write would otherwise overrun. The view classes layer their own
 * record/byte semantics on top.
 *
 * Not thread-safe. View classes serialize access externally.
 *
 * @copyright Copyright (c) 2026 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace lopcore::detail
{

class RingFileStorage
{
public:
    static constexpr size_t HEADER_SIZE = 8; // u64 writePos

    /**
     * @brief Construct over a backing file
     *
     * If the file does not exist it is created and pre-filled with
     * `max_body_bytes` zero bytes plus an 8-byte zero header. If it
     * exists, content is preserved and writePos is loaded from the header.
     *
     * @param path           Filesystem path for the backing file
     * @param max_body_bytes Body size in bytes (file is HEADER_SIZE + max_body_bytes)
     */
    RingFileStorage(std::string path, size_t max_body_bytes);

    ~RingFileStorage() = default;

    RingFileStorage(const RingFileStorage &) = delete;
    RingFileStorage &operator=(const RingFileStorage &) = delete;
    RingFileStorage(RingFileStorage &&) = delete;
    RingFileStorage &operator=(RingFileStorage &&) = delete;

    /**
     * @brief Write `len` bytes into the body at logical offset `pos`,
     *        wrapping at the body boundary if needed.
     *
     * `pos` is taken modulo `max_body_bytes`. The write may split into
     * two `fwrite`s if the chunk would cross the end of the body.
     *
     * Does NOT advance writePos. Caller must call `advanceAndPersist`
     * separately so the position is updated atomically with the persist.
     */
    void writeWrapped(uint64_t pos, const void *data, size_t len);

    /**
     * @brief Read `len` bytes from body offset `body_offset` into `out`.
     *
     * `body_offset` MUST be in [0, max_body_bytes). Does not wrap.
     */
    void readBytes(size_t body_offset, void *out, size_t len) const;

    /**
     * @brief Increment the in-memory writePos by `bytes` and persist to header.
     */
    void advanceAndPersist(uint64_t bytes);

    /**
     * @brief Set writePos to 0 and persist.
     */
    void resetAndPersist();

    uint64_t writePos() const
    {
        return write_pos_;
    }

    size_t maxBodyBytes() const
    {
        return max_body_bytes_;
    }

    const std::string &path() const
    {
        return path_;
    }

private:
    std::string path_;
    size_t      max_body_bytes_;
    uint64_t    write_pos_;

    void ensureFile();
    void loadWritePos();
    void persistWritePos();
};

} // namespace lopcore::detail
