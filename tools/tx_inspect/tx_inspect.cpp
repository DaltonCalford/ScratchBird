#include "scratchbird/engine/file.h"
#include "scratchbird/engine/txn.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace scratchbird::engine;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <seg0 path> <txn_id>\n", argv[0]);
        return 1;
    }
    std::string seg0 = argv[1];
    std::uint64_t xid = std::strtoull(argv[2], nullptr, 10);

    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = fo;
    FileMap fmap(fl);
    fmap.set_base_path(seg0.c_str(), "");

    TransactionManager tm(std::move(fmap), fl.page_size);
    auto st = tm.read_txn_state(xid);
    std::printf("txn %llu state=%u (0=Idle,1=Active,2=Committed,3=Aborted) TIP=%u\n",
                (unsigned long long)xid, (unsigned)st, tm.tip_page_no());
    return 0;
}
