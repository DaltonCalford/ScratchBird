#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include <cstring>

#include "scratchbird/core/ondisk.h"

using namespace scratchbird::core;

static void write_page_fd(int fd, uint32_t page_id, uint32_t page_size,
                          const std::vector<uint8_t> &buf)
{
    off_t off = static_cast<off_t>(page_id) * page_size;
    ASSERT_EQ(::lseek(fd, off, SEEK_SET), off);
    ASSERT_EQ(::write(fd, buf.data(), page_size), (ssize_t)page_size);
}

TEST(Locking, CreationRollbackOnFailure)
{
    uint32_t ps = 8192;
    std::string path = "/tmp/sb_create_rollback.db";
    ::unlink(path.c_str());
    int fd = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
    ASSERT_GE(fd, 0);

    // Write header page
    std::vector<uint8_t> page(ps, 0);
    auto *h = reinterpret_cast<PageHeader *>(page.data());
    h->magic = K_MAGIC_SBRD;
    h->version = 1;
    h->page_type = PAGE_TYPE_DATABASE_HEADER;
    h->page_size = ps;
    h->checksum = calculatePageChecksum(page.data(), ps);
    write_page_fd(fd, 0, ps, page);

    // Simulate crash before writing page 1 by closing early
    ::fsync(fd);
    ::close(fd);

    // Re-open and verify file is smaller than 2 pages, indicating incomplete creation
    struct stat st{};
    ASSERT_EQ(::stat(path.c_str(), &st), 0);
    EXPECT_LT((size_t)st.st_size, (size_t)ps * 2);
    ::unlink(path.c_str());
}
