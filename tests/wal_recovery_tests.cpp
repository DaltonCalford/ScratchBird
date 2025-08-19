#include "scratchbird/engine/btree_page.h"
#include "scratchbird/engine/btree_v1.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/wal.h"

#include <cassert>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace scratchbird::engine;

static CompositeKey k(int v)
{
    CompositeKey c;
    c.parts.push_back(KeyPart{std::to_string(v), false, false});
    return c;
}
static std::vector<std::uint8_t> enc(const CompositeKey& c)
{
    std::vector<std::uint8_t> out;
    detail::encode_key(c, out);
    return out;
}

int main()
{
    // Create WAL and append some ops
    std::string wal_path = std::string("/tmp/wal_") + std::to_string(::getpid());
    {
        WalManager wal(wal_path);
        wal.append_insert(enc(k(1)), 1, "");
        wal.append_insert(enc(k(2)), 2, "");
        wal.append_delete(enc(k(1)));
        wal.flush();
    }
    // Replay into a fresh tree
    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 64;
    std::string dir = std::string("/tmp/sb_replay_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);
    FileMap fmap(layout);
    fmap.set_base_path(dir, "idx");
    BTreeV1 t(std::move(fmap), layout.page_size, false);
    t.create_empty();
    WalManager wal(wal_path);
    auto recs = wal.read_all();
    for (auto& r : recs) {
        if (r.kind == WalRecKind::Insert) {
            // decode key
            CompositeKey ck;
            detail::decode_key(r.key_bytes.data(), r.key_bytes.size(), ck);
            std::string err;
            t.insert(ck, r.row_id, r.payload, err);
        } else {
            CompositeKey ck;
            detail::decode_key(r.key_bytes.data(), r.key_bytes.size(), ck);
            t.erase_equal(ck);
        }
    }
    // Expect only key 2 remaining
    std::vector<std::string> keys;
    t.inorder_keys(keys);
    assert(keys.size() == 1 && keys[0] == "2");
    // Cleanup files
    for (std::size_t i = 0; i < 4; ++i) {
        std::string p = dir + "/idx.seg" + std::to_string(i);
        ::unlink(p.c_str());
    }
    ::rmdir(dir.c_str());
    ::unlink(wal_path.c_str());
    return 0;
}
