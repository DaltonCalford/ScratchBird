#include "scratchbird/engine/btree_page.h"
#include "scratchbird/engine/btree_v1.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/wal.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <set>
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

static int decode_key_int(const std::vector<std::uint8_t>& key_bytes)
{
    CompositeKey ck;
    detail::decode_key(key_bytes.data(), key_bytes.size(), ck);
    return std::stoi(ck.parts.empty() ? std::string("0") : ck.parts[0].bytes);
}

int main()
{
    // Prepare WAL with a series of ops
    std::string wal_path = std::string("/tmp/wal_crash_") + std::to_string(::getpid());
    WalManager wal(wal_path);
    wal.set_group_commit(8);
    // Plan: Insert 1..100, delete evens
    for (int i = 1; i <= 100; ++i)
        wal.append_insert(enc(k(i)), i, "");
    for (int i = 2; i <= 100; i += 2)
        wal.append_delete(enc(k(i)));
    wal.flush();

    // Read full WAL once
    auto recs_all = wal.read_all();
    for (size_t cut = 1; cut <= recs_all.size(); cut += 17) {
        std::vector<WalRecord> recs(recs_all.begin(), recs_all.begin() + cut);
        // Reference state from WAL prefix
        std::multiset<int> ref;
        for (auto& r : recs) {
            int v = decode_key_int(r.key_bytes);
            if (r.kind == WalRecKind::Insert)
                ref.insert(v);
            else if (r.kind == WalRecKind::Delete) {
                auto it = ref.find(v);
                if (it != ref.end())
                    ref.erase(it);
            }
        }
        // Replay into fresh index
        FileMap::Layout layout{};
        layout.page_size = 4096;
        layout.pages_per_segment = 64;
        std::string dir =
            std::string("/tmp/sb_replay_cut_") + std::to_string(::getpid()) + std::to_string(cut);
        ::mkdir(dir.c_str(), 0700);
        FileMap fmap(layout);
        fmap.set_base_path(dir, "idx");
        BTreeV1 t(std::move(fmap), layout.page_size, false);
        t.create_empty();
        for (auto& r : recs) {
            CompositeKey ck;
            detail::decode_key(r.key_bytes.data(), r.key_bytes.size(), ck);
            if (r.kind == WalRecKind::Insert) {
                std::string err;
                t.insert(ck, r.row_id, r.payload, err);
            } else if (r.kind == WalRecKind::Delete) {
                t.erase_equal(ck);
            }
        }
        std::vector<std::string> keys;
        t.inorder_keys(keys);
        std::vector<int> got;
        got.reserve(keys.size());
        for (auto& s : keys)
            got.push_back(std::stoi(s));
        std::vector<int> expected(ref.begin(), ref.end());
        assert(got == expected);
        // Clean up segment files to avoid filling /tmp
        for (const auto& seg :
             t.root_page() ? std::vector<std::string>{} : std::vector<std::string>{}) {
        }
        // best-effort remove created files
        for (std::size_t i = 0; i < 4; ++i) {
            std::string p = dir + "/idx.seg" + std::to_string(i);
            ::unlink(p.c_str());
        }
        ::rmdir(dir.c_str());
    }
    // Remove WAL file
    ::unlink(wal_path.c_str());
    return 0;
}
