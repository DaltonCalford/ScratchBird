#include "scratchbird/capi.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

static std::string tmpdir()
{
    const char* d = std::getenv("TMPDIR");
    std::string base = d ? d : "/tmp";
    return base + std::string("/sb_dbhdr_") + std::to_string(::getpid());
}

int main()
{
    std::string dir = tmpdir();
    ::mkdir(dir.c_str(), 0700);
    std::string path = dir + "/dbx";

    SB_CreateDbOptions o{};
    o.page_size = 8192;
    o.default_charset = "UTF8";
    SB_Database* db = nullptr;
    auto st = sb_create_database(path.c_str(), &o, &db);
    assert(st.code == SB_STATUS_OK);
    sb_close_database(db);

    // Run dbhdr tool
    std::string cmd = std::string("./dbhdr ") + path + " >" + dir + "/out.txt";
    int rc = std::system(cmd.c_str());
    assert(WIFEXITED(rc) && WEXITSTATUS(rc) == 0);

    // Read output
    FILE* f = std::fopen((dir + "/out.txt").c_str(), "rb");
    assert(f);
    std::array<char, 4096> buf{};
    size_t n = std::fread(buf.data(), 1, buf.size(), f);
    (void)n;
    std::fclose(f);
    std::string s(buf.data());
    assert(s.find("Page size: 8192") != std::string::npos);
    assert(s.find("ScratchBird header:") != std::string::npos);
    return 0;
}
