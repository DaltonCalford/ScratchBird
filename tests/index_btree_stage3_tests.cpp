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
    std::string dir = std::string("/tmp/sb_btree_stage3_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(dir, "idx");

    BTreeIndex idx(std::move(fmap), layout.page_size, /*unique*/ false);
    idx.create_empty();

    // Insert payload variants
    for (int i = 0; i < 1000; ++i) {
        std::string s = std::to_string(i);
        std::string payload = (i % 10 == 0) ? std::string(100, 'x') : std::string();
        std::string err;
        (void)err;
        bool ok = idx.insert_with_payload(s, static_cast<std::uint64_t>(i + 1), payload, err);
        assert(ok);
    }

    // Search equal with payload
    {
        std::string s = std::to_string(500);
        std::vector<std::pair<std::uint64_t, std::string>> out;
        idx.search_equal_with_payload(s, out);
        assert(!out.empty());
    }

    // Compute stats
    auto stats = idx.compute_stats();
    assert(stats.leaf_pages > 0);

    // Rebuild offline
    idx.rebuild_offline();

    // Cleanup
    for (std::size_t i = 0; i < 4; ++i) {
        std::string p = dir + "/idx.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::rmdir(dir.c_str());
    return 0;
}
