// SPDX-License-Identifier: IDPL
#include "scratchbird/engine/btree_v1.h"
#include "scratchbird/engine/file.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    // parser benchmark stub remains; add DB create timings
    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 1024;
    std::string d = std::string("./.bench_") + std::to_string(::getpid());
    ::mkdir(d.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(d, "db");
    BTreeV1 t(std::move(fmap), layout.page_size, false);
    auto t0 = std::chrono::steady_clock::now();
    t.create_empty();
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::printf("create_empty: %lld ms\n", (long long)ms);
    // Cleanup
    for (std::size_t i = 0; i < 2; ++i) {
        std::string p = d + "/db.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::rmdir(d.c_str());
    return 0;
}
