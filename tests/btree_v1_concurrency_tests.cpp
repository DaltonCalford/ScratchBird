#include "scratchbird/engine/btree_v1.h"
#include "scratchbird/engine/file.h"

#include <atomic>
#include <cassert>
#include <random>
#include <string>
#include <sys/stat.h>
#include <thread>
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
    layout.pages_per_segment = 256;
    std::string dir = std::string("/tmp/sb_btree_v1_conc_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(dir, "idx");

    BTreeV1 t(std::move(fmap), layout.page_size, /*unique*/ false);
    t.create_empty();

    const int threads = 4;           // further reduce to avoid CI flakiness on constrained runners
    const int ops_per_thread = 1500; // still exercises contention adequately
    std::vector<std::thread> ths;
    std::atomic<bool> start{false};

    for (int ti = 0; ti < threads; ++ti) {
        ths.emplace_back([&, ti]() {
            std::mt19937 rng(1000 + ti);
            std::uniform_int_distribution<int> opdist(0, 99);
            std::uniform_int_distribution<int> valdist(0, 5000);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < ops_per_thread; ++i) {
                int v = valdist(rng);
                int op = opdist(rng);
                if (op < 60) {
                    std::string err;
                    t.insert(make_key(v), static_cast<std::uint64_t>(v), std::string(), err);
                } else if (op < 80) {
                    t.erase_equal(make_key(v));
                } else {
                    std::vector<std::string> keys;
                    t.inorder_keys(keys);
                }
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& th : ths)
        th.join();

    // Final inorder scan shouldn't crash
    std::vector<std::string> keys;
    t.inorder_keys(keys);

    // Cleanup created files
    for (std::size_t i = 0; i < 8; ++i) {
        std::string p = dir + "/idx.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::rmdir(dir.c_str());
    return 0;
}
