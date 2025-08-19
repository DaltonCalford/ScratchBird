#include "scratchbird/capi.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

static std::string tmpdir()
{
    const char* d = std::getenv("TMPDIR");
    std::string base = d ? d : "/tmp";
    return base + std::string("/sb_bootstrap_") + std::to_string(::getpid());
}

static std::string read_file(const std::string& p)
{
    FILE* f = std::fopen(p.c_str(), "rb");
    assert(f);
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    return out;
}

int main()
{
    std::string dir = tmpdir();
    ::mkdir(dir.c_str(), 0700);
    std::string path = dir + "/dbx";

    SB_CreateDbOptions o{};
    o.page_size = 4096;
    o.default_charset = "UTF8";
    SB_Database* db = nullptr;
    auto st = sb_create_database(path.c_str(), &o, &db);
    assert(st.code == SB_STATUS_OK);
    // Keep database handle alive until after we read sidecar to avoid any teardown ordering issues

    // Check sidecar exists and has replaced UUIDs
    std::string sidecar = path + ".bootstrap.sql";
    struct stat stbuf{};
    assert(::stat(sidecar.c_str(), &stbuf) == 0);
    auto text = read_file(sidecar);
    assert(text.find("sys.catalog") != std::string::npos);
    assert(text.find("0x") != std::string::npos);
    assert(text.find("<SYS_CATALOG_UUID>") == std::string::npos);
    // Cleanup handle
    sb_close_database(db);
    return 0;
}
