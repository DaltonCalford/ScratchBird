#include "scratchbird/capi.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace scratchbird::engine;

static std::string tmpdir()
{
    const char* d = std::getenv("TMPDIR");
    std::string base = d ? d : "/tmp";
    return base + std::string("/sb_capi_") + std::to_string(::getpid());
}

int main()
{
    std::string dir = tmpdir();
    ::mkdir(dir.c_str(), 0700);

    uint32_t sizes[] = {4096u, 8192u, 16384u, 32768u, 65536u, 131072u};
    for (auto ps : sizes) {
        std::string base = dir + "/db" + std::to_string(ps);
        SB_CreateDbOptions o{};
        o.page_size = ps;
        o.default_charset = "UTF8";
        o.page_cache = 0;
        o.sweep_interval = 0;
        o.reserve_space = 0;
        SB_Database* db = nullptr;
        auto st = sb_create_database(base.c_str(), &o, &db);
        assert(st.code == SB_STATUS_OK);
        sb_close_database(db);
        // open
        st = sb_open_database(base.c_str(), &db);
        assert(st.code == SB_STATUS_OK);
        sb_close_database(db);
    }

    // Unsupported page size
    {
        std::string base = dir + "/db_bad";
        SB_CreateDbOptions o{};
        o.page_size = 4096;
        SB_Database* db = nullptr;
        auto st = sb_create_database(base.c_str(), &o, &db);
        assert(st.code == SB_STATUS_OK);
        sb_close_database(db);
        // corrupt header page_size to an unsupported value
        FileMap::Layout layout{};
        layout.page_size = 4096;
        FileMap fmap(layout);
        fmap.set_base_path(dir, "db_bad");
        std::vector<uint8_t> page(4096, 0);
        FileOptions fo{};
        auto fh = FileManager::open(dir + "/db_bad.seg0", fo, false);
        FileManager::pread(fh, page.data(), page.size(), 0);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_size = 12345;
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        FileManager::pwrite(fh, page.data(), page.size(), 0);
        st = sb_open_database(base.c_str(), &db);
        assert(st.code == SB_STATUS_ERROR);
    }

    // Checksum mismatch
    {
        std::string base = dir + "/db_bad2";
        SB_CreateDbOptions o{};
        o.page_size = 4096;
        SB_Database* db = nullptr;
        auto st = sb_create_database(base.c_str(), &o, &db);
        assert(st.code == SB_STATUS_OK);
        sb_close_database(db);
        FileOptions fo{};
        auto fh = FileManager::open(dir + "/db_bad2.seg0", fo, false);
        std::vector<uint8_t> page(4096, 0);
        FileManager::pread(fh, page.data(), page.size(), 0);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->checksum ^= 0xFFFF; // break it
        FileManager::pwrite(fh, page.data(), page.size(), 0);
        st = sb_open_database(base.c_str(), &db);
        assert(st.code == SB_STATUS_ERROR);
    }

    // Cleanup temp dir not strictly needed
    return 0;
}
