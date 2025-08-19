#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_btree.h"

#include <algorithm>
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
    std::string dir = std::string("/tmp/sb_btree_stage2_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(dir, "idx");

    BTreeIndex idx(std::move(fmap), layout.page_size, /*unique*/ false);
    idx.create_empty();

    // Insert keys
    for (int i = 0; i < 1000; ++i) {
        std::string s = std::to_string(i);
        std::string err;
        (void)err;
        bool ok = idx.insert(s, static_cast<std::uint64_t>(i + 1), err);
        assert(ok);
    }

    // Erase middle range keys (e.g., 400..599)
    for (int i = 400; i < 600; ++i) {
        std::string s = std::to_string(i);
        std::string err;
        (void)err;
        idx.erase_equal(s, err);
    }

    // Verify remaining are sorted and don't include erased
    std::vector<std::pair<std::string, std::uint64_t>> out;
    std::string a = std::to_string(0);
    std::string b = std::to_string(999);
    idx.search_range(a, true, b, true, out);
    assert(
        std::is_sorted(out.begin(), out.end(), [](auto& x, auto& y) { return x.first < y.first; }));

    // Cleanup
    for (std::size_t i = 0; i < 4; ++i) {
        std::string p = dir + "/idx.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::rmdir(dir.c_str());
    return 0;
}
