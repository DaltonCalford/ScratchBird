#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_btree.h"

#include <cassert>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 64;
    std::string dir = std::string("/tmp/sb_btree_stage1_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(dir, "idx");

    BTreeIndex idx(std::move(fmap), layout.page_size, /*unique*/ true);
    idx.create_empty();

    // Insert a bunch of distinct keys
    for (int i = 0; i < 1000; ++i) {
        std::string s = std::to_string(i);
        std::string err;
        bool ok = idx.insert(s, static_cast<std::uint64_t>(i + 1), err);
        assert(ok);
    }

    // Duplicate insert should fail for unique index
    {
        std::string s = std::to_string(500);
        std::string err;
        bool ok = idx.insert(s, 9999, err);
        assert(!ok);
    }

    // Search equal
    {
        std::string s = std::to_string(123);
        std::vector<std::uint64_t> out;
        idx.search_equal(s, out);
        assert(out.size() == 1 && out[0] == 124);
    }

    // Range search
    {
        std::vector<std::pair<std::string, std::uint64_t>> out;
        std::string a = std::to_string(100);
        std::string b = std::to_string(199);
        idx.search_range(a, true, b, true, out);
        assert(out.size() >= 100);
    }

    // Cleanup
    for (std::size_t i = 0; i < 4; ++i) {
        std::string p = dir + "/idx.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::rmdir(dir.c_str());
    return 0;
}
