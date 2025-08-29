#include "tests/chaos/fault_inject.h"
#include "scratchbird/engine/file.h"
#include <iostream>

int main() {
    using namespace scratchbird::engine;
    try {
        // Inject a one-shot disk failure
        scratchbird::chaos::FaultInjector::instance().set("disk_pwrite_fail", 1);
        FileOptions opts{};
        opts.direct_io = false;
        opts.preallocate_bytes = 0;
        auto fh = FileManager::open("/tmp/sb_chaos_disk", opts, true);
        SB_FAULT("disk_pwrite_fail");
        const char buf[4096] = {0};
        FileManager::pwrite(fh, buf, sizeof(buf), 0);
        FileManager::flush(fh);
        std::cout << "No fault triggered" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cout << "Caught injected fault: " << ex.what() << std::endl;
        return 0;
    }
}

