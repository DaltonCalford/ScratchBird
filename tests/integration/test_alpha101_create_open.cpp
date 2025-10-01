#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

static bool file_exists(const std::string &path)
{
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0;
}

static size_t file_size(const std::string &path)
{
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0)
        return 0;
    return static_cast<size_t>(st.st_size);
}

static void write_page(int fd, uint32_t page_id, uint32_t page_size,
                       const std::vector<uint8_t> &buf)
{
    off_t off = static_cast<off_t>(page_id) * page_size;
    ASSERT_EQ(::lseek(fd, off, SEEK_SET), off);
    ASSERT_EQ(::write(fd, buf.data(), page_size), static_cast<ssize_t>(page_size));
}

static void read_page(int fd, uint32_t page_id, uint32_t page_size, std::vector<uint8_t> &out)
{
    off_t off = static_cast<off_t>(page_id) * page_size;
    ASSERT_EQ(::lseek(fd, off, SEEK_SET), off);
    out.resize(page_size);
    ASSERT_EQ(::read(fd, out.data(), page_size), static_cast<ssize_t>(page_size));
}

static void init_db_header_page(std::vector<uint8_t> &page, uint32_t page_size)
{
    page.assign(page_size, 0);
    auto *h = reinterpret_cast<PageHeader *>(page.data());
    h->magic = K_MAGIC_SBRD;
    h->version = 1;
    h->page_type = PAGE_TYPE_DATABASE_HEADER;
    h->page_size = page_size;
    h->lsn = 0;
    h->page_id = 0;
    h->flags = 0;
    auto uuid = generateUuidV7();
    memcpy(h->database_uuid, uuid.bytes.data(), uuid.bytes.size());
    h->generation = 1;
    h->free_space = 0;
    h->item_count = 0;
    h->free_offset = sizeof(PageHeader);
    h->special_size = 0;
    h->checksum = calculatePageChecksum(page.data(), page_size);
}

TEST(Alpha101, CreateDatabaseFileStructure)
{
    for (uint32_t ps : {8192u, 16384u, 32768u, 65536u, 131072u})
    {
        std::string path = "/tmp/sb_alpha101_" + std::to_string(ps) + ".db";
        ::unlink(path.c_str());

        int fd = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
        ASSERT_GE(fd, 0);

        // Allocate minimum 2 pages: DB header (0), system catalog root (1)
        std::vector<uint8_t> page;
        init_db_header_page(page, ps);
        write_page(fd, 0, ps, page);

        page.assign(ps, 0);
        auto *h1 = reinterpret_cast<PageHeader *>(page.data());
        h1->magic = K_MAGIC_SBRD;
        h1->version = 1;
        h1->page_type = PAGE_TYPE_SYSTEM_CATALOG;
        h1->page_size = ps;
        h1->lsn = 0;
        h1->page_id = 1;
        h1->flags = 0;
        memset(h1->database_uuid, 0xAB, 16); // placeholder identity
        h1->generation = 1;
        h1->free_space = 0;
        h1->item_count = 0;
        h1->free_offset = sizeof(PageHeader);
        h1->special_size = 0;
        h1->checksum = calculatePageChecksum(page.data(), ps);
        write_page(fd, 1, ps, page);

        ::fsync(fd);
        ::close(fd);

        ASSERT_TRUE(file_exists(path));
        ASSERT_EQ(file_size(path), static_cast<size_t>(ps) * 2);

        // Validate header page
        fd = ::open(path.c_str(), O_RDONLY);
        ASSERT_GE(fd, 0);
        std::vector<uint8_t> readbuf;
        read_page(fd, 0, ps, readbuf);
        auto *hdr = reinterpret_cast<const PageHeader *>(readbuf.data());
        EXPECT_EQ(hdr->magic, K_MAGIC_SBRD);
        EXPECT_EQ(hdr->page_id, 0u);
        EXPECT_EQ(hdr->page_type, PAGE_TYPE_DATABASE_HEADER);
        EXPECT_TRUE(validatePageChecksum(readbuf.data(), ps));

        // Validate system catalog page
        read_page(fd, 1, ps, readbuf);
        auto *sys = reinterpret_cast<const PageHeader *>(readbuf.data());
        EXPECT_EQ(sys->magic, K_MAGIC_SBRD);
        EXPECT_EQ(sys->page_id, 1u);
        EXPECT_EQ(sys->page_type, PAGE_TYPE_SYSTEM_CATALOG);
        EXPECT_TRUE(validatePageChecksum(readbuf.data(), ps));
        ::close(fd);

        ::unlink(path.c_str());
    }
}