#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_btree.h"
#include "scratchbird/engine/ods.h"

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
    layout.pages_per_segment = 4; // keep tiny to avoid prealloc
    // create dedicated subdir under /tmp
    std::string dir = std::string("/tmp/sb_btree_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(dir, "db");
    // Sanity: ensure we can create a file in this dir
    {
        FileOptions opts{};
        auto fh = FileManager::open(dir + "/probe", opts, true);
        (void)fh;
    }

    // Build with many keys to trigger multiple leaves
    BTreeBuilder builder(std::move(fmap), layout.page_size);
    std::vector<BTreeKeyRef> keys;
    for (int i = 0; i < 200; ++i) {
        keys.push_back({std::string(10, char('a' + (i % 26))), static_cast<std::uint64_t>(i + 1)});
    }
    auto res = builder.build(keys);
    assert(res.key_count == keys.size());
    assert(res.leaf_pages >= 2);
    assert(res.root_page != 0);

    // We cannot easily read pages without exposing FileMap. Counts asserted above are sufficient
    // here. Cleanup
    for (std::size_t i = 0; i < 2; ++i) {
        std::string p = dir + "/db.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::unlink((dir + "/probe").c_str());
    ::rmdir(dir.c_str());
    return 0;
}
