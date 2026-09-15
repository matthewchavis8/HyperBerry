#include "cpio.h"
#include "lib/strings/strings.h"

namespace {
const char* normalize(const char* name) {
    while (name[0] == '.' && name[1] == '/')
        name += 2;
    return name;
}

bool validPath(const char* name) {
    if (!*name || *name == '/') return false;
    if (StrEq(name, ".")) return true;
    while (*name) {
        const char* start { name };
        while (*name && *name != '/')
            ++name;
        uint64_t length { static_cast<uint64_t>(name - start) };
        if (!length || (length == 1 && start[0] == '.') ||
                (length == 2 && start[0] == '.' && start[1] == '.'))
            return false;
        if (*name && !*++name) return false;
    }
    return true;
}
} // namespace

namespace cpio {
Archive::Archive(const void* data, uint64_t size) :
            m_data { static_cast<const uint8_t*>(data) }, m_size { size } {
    if (data && size <= UINTPTR_MAX - reinterpret_cast<uintptr_t>(data)) m_error = validate();
}

Error Archive::read(uint64_t& cursor, Entry& entry) const {
    if (cursor > m_size || m_size - cursor < 110) return Error::TRUNCATED;
    const uint8_t* header { m_data + cursor };
    const char magic[] { "070701" };
    for (unsigned i {}; i < 6; ++i)
        if (header[i] != magic[i]) return Error::BAD_MAGIC;
    uint32_t fields[13] {};
    for (unsigned i {}; i < 13; ++i) {
        for (unsigned j {}; j < 8; ++j) {
            uint8_t c { header[6 + i * 8 + j] };
            unsigned digit {};
            if (c >= '0' && c <= '9')
                digit = c - '0';
            else if (c >= 'a' && c <= 'f')
                digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                digit = c - 'A' + 10;
            else
                return Error::BAD_FIELD;
            fields[i] = (fields[i] << 4) | digit;
        }
    }
    if (fields[12] != 0) return Error::BAD_FIELD;
    cursor += 110;
    uint64_t nameSize { fields[11] };
    if (!nameSize || nameSize > m_size - cursor) return Error::BAD_NAME;
    const char* name { reinterpret_cast<const char*>(m_data + cursor) };
    uint64_t length {};
    while (length < nameSize && name[length])
        ++length;
    if (length == nameSize) return Error::BAD_NAME;
    for (uint64_t i { length }; i < nameSize; ++i)
        if (name[i]) return Error::BAD_NAME;
    name = normalize(name);
    if (!validPath(name)) return Error::BAD_NAME;
    cursor += nameSize;
    uint64_t padding { (4 - (cursor & 3)) & 3 };
    if (padding > m_size - cursor) return Error::TRUNCATED;
    cursor += padding;
    uint64_t fileSize { fields[6] };
    if (fileSize > m_size - cursor) return Error::TRUNCATED;
    entry = { name, { m_data + cursor, fileSize }, (fields[1] & 0170000) == 0040000 };
    cursor += fileSize;
    padding = (4 - (cursor & 3)) & 3;
    if (padding > m_size - cursor) return Error::TRUNCATED;
    cursor += padding;
    if (StrEq(name, "TRAILER!!!")) return fileSize ? Error::BAD_TRAILER : Error::NONE;
    if (entry.isDirectory) {
        if (fileSize) return Error::BAD_FIELD;
    } else if ((fields[1] & 0170000) != 0100000 || fields[4] != 1 || StrEq(name, ".")) {
        return Error::UNSUPPORTED_TYPE;
    }
    return Error::NONE;
}

Error Archive::validate() {
    uint64_t cursor {};
    while (cursor < m_size) {
        uint64_t start { cursor };
        Entry entry {};
        Error error { read(cursor, entry) };
        if (error != Error::NONE) return error;
        if (StrEq(entry.name, "TRAILER!!!")) {
            for (uint64_t i { cursor }; i < m_size; ++i)
                if (m_data[i]) return Error::BAD_TRAILER;
            m_end = start;
            return Error::NONE;
        }
        uint64_t previous {};
        while (previous < start) {
            Entry other {};
            error = read(previous, other);
            if (error != Error::NONE) return error;
            if (StrEq(other.name, entry.name)) return Error::DUPLICATE_PATH;
        }
    }
    return Error::MISSING_TRAILER;
}

bool Archive::Next(uint64_t& cursor, Entry& entry) const {
    entry = {};
    if (m_error != Error::NONE || cursor >= m_end) return false;
    return read(cursor, entry) == Error::NONE;
}

bool Archive::Find(const char* path, File& file) const {
    file = {};
    if (!path) return false;
    path = normalize(path);
    uint64_t cursor {};
    Entry entry {};
    while (Next(cursor, entry)) {
        if (!entry.isDirectory && StrEq(entry.name, path)) {
            file = entry.file;
            return true;
        }
    }
    return false;
}
} // namespace cpio
