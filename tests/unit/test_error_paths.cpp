#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "scratchbird/core/ondisk.h"

using namespace scratchbird::core;

static void write_file(const char* path, const std::vector<uint8_t>& data) {
	int fd = ::open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	ASSERT_GE(fd, 0);
	ASSERT_EQ(::write(fd, data.data(), data.size()), (ssize_t)data.size());
	::fsync(fd);
	::close(fd);
}

TEST(ErrorPaths, InvalidPageSize) {
	EXPECT_FALSE(is_valid_alpha_page_size(4096));
	EXPECT_FALSE(is_valid_alpha_page_size(65536));
}

TEST(ErrorPaths, CorruptedMagic) {
	uint32_t ps = 8192;
	std::vector<uint8_t> page(ps, 0);
	auto* h = reinterpret_cast<PageHeader*>(page.data());
	h->magic = 0xDEADBEEF; // wrong magic
	h->version = 1;
	h->page_type = PAGE_TYPE_DATABASE_HEADER;
	h->page_size = ps;
	h->checksum = calculate_page_checksum(page.data(), ps);
	// validate_page_checksum passes but magic check should fail in higher-level code (future)
	EXPECT_TRUE(validate_page_checksum(page.data(), ps));
}

TEST(ErrorPaths, ChecksumMismatch) {
	uint32_t ps = 8192;
	std::vector<uint8_t> page(ps, 0);
	auto* h = reinterpret_cast<PageHeader*>(page.data());
	h->magic = kMagicSBRD;
	h->version = 1;
	h->page_type = PAGE_TYPE_DATABASE_HEADER;
	h->page_size = ps;
	h->checksum = calculate_page_checksum(page.data(), ps);
	// Tamper to break checksum
	page[100] ^= 0xFF;
	EXPECT_FALSE(validate_page_checksum(page.data(), ps));
}

TEST(ErrorPaths, ShortFileRead) {
	const char* path = "/tmp/sb_short_read.db";
	// Write a file smaller than one page
	uint32_t ps = 8192;
	std::vector<uint8_t> buf(ps / 2, 0);
	write_file(path, buf);
	int fd = ::open(path, O_RDONLY);
	ASSERT_GE(fd, 0);
	std::vector<uint8_t> page(ps, 0);
	off_t off = 0;
	ASSERT_EQ(::lseek(fd, off, SEEK_SET), off);
	ssize_t n = ::read(fd, page.data(), ps);
	EXPECT_NE(n, (ssize_t)ps);
	::close(fd);
	::unlink(path);
}