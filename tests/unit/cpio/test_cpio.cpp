#include <gtest/gtest.h>
#include <fstream>
#include <iterator>
#include "lib/cpio/cpio.h"
#include "fixture.h"

namespace {
std::vector<uint8_t> archive() {
    std::vector<uint8_t> bytes;
    fixture::Entry(bytes, "./linux", {}, 0040755);
    fixture::Entry(bytes, "./linux/Image", { 1, 2, 3 });
    fixture::Entry(bytes, "empty");
    fixture::Finish(bytes);
    return bytes;
}
} // namespace

TEST(Cpio, FindsFilesAndIteratesDirectories) {
    auto bytes { archive() };
    bytes.resize(bytes.size() + 512, 0);
    cpio::Archive archive { bytes.data(), bytes.size() };
    ASSERT_EQ(archive.GetError(), cpio::Error::NONE);
    cpio::File file {};
    ASSERT_TRUE(archive.Find("linux/Image", file));
    EXPECT_EQ(file.size, 3);
    EXPECT_EQ(file.data[2], 3);
    EXPECT_FALSE(archive.Find("linux", file));
    EXPECT_TRUE(archive.Find("empty", file));
    EXPECT_EQ(file.size, 0);
    uint64_t cursor {};
    cpio::Entry entry {};
    unsigned count {};
    while (archive.Next(cursor, entry))
        ++count;
    EXPECT_EQ(count, 3);
}

TEST(Cpio, RejectsEveryTruncationBeforeTrailerEnds) {
    auto bytes { archive() };
    for (size_t size {}; size < bytes.size(); ++size)
        EXPECT_NE(cpio::Archive(bytes.data(), size).GetError(), cpio::Error::NONE) << size;
}

TEST(Cpio, RejectsInvalidRegions) {
    EXPECT_EQ(cpio::Archive(nullptr, 10).GetError(), cpio::Error::INVALID_REGION);
    uint8_t byte {};
    EXPECT_EQ(cpio::Archive(&byte, UINT64_MAX).GetError(), cpio::Error::INVALID_REGION);
}

TEST(Cpio, RejectsBadHeaders) {
    for (size_t offset : { 0U, 6U, 14U, 54U, 94U, 102U }) {
        auto bytes { archive() };
        bytes[offset] = 'z';
        EXPECT_NE(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::NONE);
    }
    auto bytes { archive() };
    bytes[5] = '2';
    EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::BAD_MAGIC);
}

TEST(Cpio, RejectsBadNamesAndDuplicateNormalizedPaths) {
    for (const char* name : { "/absolute", "../escape", "a/../b", "a//b", "a/", "" }) {
        std::vector<uint8_t> bytes;
        fixture::Entry(bytes, name);
        fixture::Finish(bytes);
        EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::BAD_NAME);
    }
    std::vector<uint8_t> bytes;
    fixture::Entry(bytes, "./file");
    fixture::Entry(bytes, "file");
    fixture::Finish(bytes);
    EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::DUPLICATE_PATH);
}

TEST(Cpio, RejectsUnsupportedTypesAndHardLinks) {
    for (uint32_t mode : { 0120777U, 0020600U, 0060600U, 0010600U }) {
        std::vector<uint8_t> bytes;
        fixture::Entry(bytes, "file", {}, mode);
        fixture::Finish(bytes);
        EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(),
                cpio::Error::UNSUPPORTED_TYPE);
    }
    std::vector<uint8_t> bytes;
    fixture::Entry(bytes, "file");
    fixture::Field(bytes, 38, 2);
    fixture::Finish(bytes);
    EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::UNSUPPORTED_TYPE);
}

TEST(Cpio, RejectsInvalidLengthsAndUnterminatedNames) {
    for (size_t offset : { 54U, 94U }) {
        auto bytes { archive() };
        fixture::Field(bytes, offset, UINT32_MAX);
        EXPECT_NE(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::NONE);
    }
    auto bytes { archive() };
    bytes[117] = 'x';
    EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::BAD_NAME);
}

TEST(Cpio, ValidatesBeyondRequestedFile) {
    auto bytes { archive() };
    bytes.push_back(1);
    cpio::Archive archive { bytes.data(), bytes.size() };
    EXPECT_EQ(archive.GetError(), cpio::Error::BAD_TRAILER);
    cpio::File file {};
    EXPECT_FALSE(archive.Find("linux/Image", file));
}

TEST(Cpio, RequiresTrailerAndRejectsTrailerData) {
    std::vector<uint8_t> bytes;
    fixture::Entry(bytes, "file");
    EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::MISSING_TRAILER);
    fixture::Entry(bytes, "TRAILER!!!", { 1 }, 0);
    EXPECT_EQ(cpio::Archive(bytes.data(), bytes.size()).GetError(), cpio::Error::BAD_TRAILER);
}

TEST(Cpio, ReadsArchiveProducedBySystemCpio) {
    std::ifstream input(CPIO_FIXTURE, std::ios::binary);
    ASSERT_TRUE(input.good());
    std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(input), {} };
    cpio::Archive archive { bytes.data(), bytes.size() };
    ASSERT_EQ(archive.GetError(), cpio::Error::NONE);
    cpio::File file {};
    ASSERT_TRUE(archive.Find("message.txt", file));
    ASSERT_EQ(file.size, 5);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(file.data), file.size), "cpio\n");
}
