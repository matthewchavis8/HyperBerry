#ifndef __FIXTURE_H__
#define __FIXTURE_H__

#include <cstdio>
#include <string>
#include <vector>
#include <stdint.h>

namespace fixture {
inline void Field(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    char hex[9] {};
    std::snprintf(hex, sizeof(hex), "%08x", value);
    for (size_t i {}; i < 8; ++i)
        bytes[offset + i] = hex[i];
}

inline void Entry(std::vector<uint8_t>& bytes,
        const std::string& name,
        const std::vector<uint8_t>& data = {},
        uint32_t mode = 0100644) {
    size_t start { bytes.size() };
    bytes.resize(start + 110, '0');
    const std::string magic { "070701" };
    for (size_t i {}; i < 6; ++i)
        bytes[start + i] = magic[i];
    Field(bytes, start + 14, mode);
    Field(bytes, start + 38, 1);
    Field(bytes, start + 54, data.size());
    Field(bytes, start + 94, name.size() + 1);
    bytes.insert(bytes.end(), name.begin(), name.end());
    bytes.push_back(0);
    while (bytes.size() % 4)
        bytes.push_back(0);
    bytes.insert(bytes.end(), data.begin(), data.end());
    while (bytes.size() % 4)
        bytes.push_back(0);
}

inline void Finish(std::vector<uint8_t>& bytes) {
    Entry(bytes, "TRAILER!!!", {}, 0);
}
} // namespace fixture

#endif // !__FIXTURE_H__
