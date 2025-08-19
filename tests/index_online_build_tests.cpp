#include "scratchbird/engine/index_online.h"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>
#include <unistd.h>

using namespace scratchbird::engine;

int main()
{
    IndexDefinition def{};
    def.index_name = "idx_onl";
    def.relation_name = "t";
    def.unique = false;
    IndexBuildOptions opts{};
    opts.page_size = 4096;
    opts.online = true;
    std::string base = std::string("/tmp/idx_onl_") + std::to_string(::getpid());

    ConcurrentIndexBuild build(def, opts, base);

    // Simulate writer thread that generates deltas
    std::thread writer([&] {
        for (int i = 0; i < 100; ++i) {
            build.register_delta(IndexDelta{std::string("k") + std::to_string(i),
                                            static_cast<std::uint64_t>(i), false});
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Backfill over base rows (emit some initial rows)
    build.run_backfill([&](auto const& emit) {
        for (int i = 0; i < 100; ++i)
            emit(std::string("b") + std::to_string(i), static_cast<std::uint64_t>(1000 + i));
    });

    // Catch-up until quiet
    build.run_catchup_until_quiet(std::chrono::milliseconds(50), 10);

    // Fence and swap using no-op fence callbacks
    build.fence_and_swap([] {}, [] {});

    writer.join();
    assert(build.state() == OnlineBuildState::Swapped);
    return 0;
}
