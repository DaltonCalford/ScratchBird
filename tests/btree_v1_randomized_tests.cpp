#include "scratchbird/engine/btree_v1.h"
#include "scratchbird/engine/file.h"

#include <algorithm>
#include <cassert>
#include <random>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace scratchbird::engine;

static CompositeKey make_key(int v)
{
    CompositeKey k;
    k.parts.push_back(KeyPart{std::to_string(v), false, false});
    return k;
}

int main()
{
    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 128;
    std::string dir = std::string("/tmp/sb_btree_v1_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(dir, "idx");

    BTreeV1 t(std::move(fmap), layout.page_size, /*unique*/ false);
    t.create_empty();

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> opdist(0, 99);
    std::uniform_int_distribution<int> valdist(0, 999);

    std::multiset<int> ref;
    const int ops = 2000; // keep small for CI while debugging
    for (int i = 0; i < ops; ++i) {
        int v = valdist(rng);
        int op = opdist(rng);
        if (op < 70) { // insert
            std::string err;
            t.insert(make_key(v), static_cast<std::uint64_t>(v), std::string(), err);
            ref.insert(v);
        } else { // erase some
            auto cnt = ref.count(v);
            if (cnt) {
                t.erase_equal(make_key(v));
                ref.erase(v);
            }
        }
    }

    // Compare inorder traversal
    std::vector<std::string> keys;
    t.inorder_keys(keys);
    std::vector<int> got;
    got.reserve(keys.size());
    for (auto& s : keys)
        got.push_back(std::stoi(s));
    std::vector<int> expect(ref.begin(), ref.end());
    assert(got == expect);

    // Cleanup created files
    for (std::size_t i = 0; i < 6; ++i) {
        std::string p = dir + "/idx.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::rmdir(dir.c_str());
    return 0;
}
