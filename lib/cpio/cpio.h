#pragma once

#include <stdint.h>

namespace cpio {
enum class Error {
    NONE,
    INVALID_REGION,
    TRUNCATED,
    BAD_MAGIC,
    BAD_FIELD,
    BAD_NAME,
    UNSUPPORTED_TYPE,
    DUPLICATE_PATH,
    MISSING_TRAILER,
    BAD_TRAILER,
};

struct File {
    const uint8_t* data {};
    uint64_t size {};
};

struct Entry {
    const char* name {};
    File file {};
    bool isDirectory {};
};

// Views borrow the archive bytes, which must remain alive and immutable.
class Archive {
    const uint8_t* m_data {};
    uint64_t m_size {};
    uint64_t m_end {};
    Error m_error { Error::INVALID_REGION };

    Error read(uint64_t& cursor, Entry& entry) const;
    Error validate();

public:
    Archive(const void* data, uint64_t size);
    Error GetError() const { return m_error; }
    bool Next(uint64_t& cursor, Entry& entry) const;
    bool Find(const char* path, File& file) const;
};
} // namespace cpio
