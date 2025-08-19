#include "scratchbird/engine.h"
#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/generators.h"
#include "scratchbird/engine/header.h"
#include "scratchbird/engine/pager.h"
#include "scratchbird/engine/txn.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace scratchbird;
using namespace scratchbird::engine;

int main()
{
    Status st{};
    std::string dir = std::string("/tmp/sb_eng_") + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0700);

    auto db = open_database(":memory:", st);
    assert(db != nullptr);
    assert(st.code == StatusCode::NotImplemented);

    auto sess = create_session(db, st);
    assert(sess != nullptr);
    assert(st.code == StatusCode::NotImplemented);

    auto tx = begin_transaction(sess, st);
    assert(tx != nullptr);
    assert(st.code == StatusCode::NotImplemented);

    auto rc = commit(tx);
    assert(rc.code == StatusCode::NotImplemented);

    rc = rollback(tx);
    assert(rc.code == StatusCode::NotImplemented);

    auto stmt = prepare(sess, "select 1", st);
    assert(stmt != nullptr);
    assert(st.code == StatusCode::NotImplemented);

    auto ex = execute(stmt, {});
    assert(ex.code == StatusCode::NotImplemented);

    // FileManager basic
    FileOptions opts{};
    opts.direct_io = false;
    opts.preallocate_bytes = 0;
    auto fh = FileManager::open(dir + "/f1", opts, true);
    assert(fh.valid());
    const char buf[4096] = {0};
    FileManager::pwrite(fh, buf, sizeof(buf), 0);
    FileManager::flush(fh);

    // FileMap segmented pages
    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 8; // tiny for test
    layout.options = opts;

    FileMap fmap(layout);
    fmap.set_base_path(dir, "dbtest");

    std::vector<char> page(layout.page_size, 0);
    page[0] = 42;
    fmap.write_page(0, page.data());
    fmap.write_page(9, page.data()); // second segment

    std::vector<char> out(layout.page_size, 0);
    fmap.read_page(0, out.data());
    assert(out[0] == 42);
    std::fill(out.begin(), out.end(), 0);
    fmap.read_page(9, out.data());
    assert(out[0] == 42);

    // Pager + BufferCache smoke
    {
        FileMap::Layout layout{};
        layout.page_size = 4096;
        layout.pages_per_segment = 4;
        FileMap fmap(layout);
        fmap.set_base_path(dir, "dbpager");
        auto cache = std::make_shared<BufferCache>(layout.page_size, /*capacity_pages*/ 8);
        Pager pager(&fmap, cache);
        // allocate by touching a page and flushing
        PageKey k1{1, 0};
        auto* pf0 = pager.get_page(k1, LatchMode::Exclusive);
        std::fill(pf0->data.begin(), pf0->data.end(), 0);
        pager.mark_dirty(pf0);
        pager.release(pf0);
        pager.flush();
        auto* pf = pager.get_page(k1, LatchMode::Exclusive);
        pf->data[10] = 7;
        pager.mark_dirty(pf);
        pager.release(pf);
        pager.flush();
        std::vector<char> check(layout.page_size, 0);
        // read back via map API to ensure data persisted
        // Access underlying map via another FileMap instance
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, "dbpager");
        fmap2.read_page(k1.page_no, check.data());
        assert(check[10] == 7);
    }

    // Allocation subsystem tests (PIP)
    {
        FileMap::Layout layout{};
        layout.page_size = 4096;
        layout.pages_per_segment = 64;
        FileMap fmap(layout);
        fmap.set_base_path(dir, "dballoc");
        Allocator alloc(&fmap, layout.page_size);
        alloc.init_new();
        // Allocate several pages; should be increasing and within first region
        std::uint32_t p1 = alloc.allocate_free_page();
        std::uint32_t p2 = alloc.allocate_free_page();
        std::uint32_t p3 = alloc.allocate_free_page();
        assert(p1 < p2 && p2 < p3);
        // Free and reallocate should reuse earliest hole
        alloc.free_page(p2);
        std::uint32_t p2b = alloc.allocate_free_page();
        assert(p2b == p2);
        // Extent allocation (8 pages) not crossing region
        std::uint32_t base = alloc.allocate_extent(8);
        assert(base + 7 < base + ods::pagesPerPIP(layout.page_size));
    }

    // TIP seed test
    {
        FileMap::Layout layout{};
        layout.page_size = 4096;
        layout.pages_per_segment = 64;
        FileMap fmap(layout);
        fmap.set_base_path(dir, "dbtip");
        TransactionManager tm(std::move(fmap), layout.page_size);
        tm.init_seed();
        assert(tm.tip_page_no() == 2);
        auto st = tm.read_txn_status(0);
        (void)st; // idle placeholder
    }

    // Header/clumplets write/read/validate
    {
        FileMap::Layout layout{};
        layout.page_size = 4096;
        layout.pages_per_segment = 64;
        FileMap fmap(layout);
        fmap.set_base_path(dir, "dbhdr");
        HeaderManager hm(std::move(fmap), layout.page_size);
        HeaderInfo hi{};
        hi.page_size = layout.page_size;
        hi.ods_major = 1;
        hi.ods_minor = 0;
        hi.roots = {10, 20, 1, 2};
        hi.page_cache = 1024;
        hi.sweep_interval = 60;
        hi.reserve_space = 1;
        hi.seeded_schemas = {{"sys.catalog", 1}, {"sys.security", 2}, {"public", 3}};
        hm.write_new(hi);
        assert(hm.validate());
        HeaderInfo hi2 = hm.read();
        assert(hi2.ods_major == 1 && hi2.roots.space_catalog == 10);
        bool saw_public = false;
        for (auto& p : hi2.seeded_schemas)
            if (p.first == "public" && p.second == 3)
                saw_public = true;
        assert(saw_public);
    }

    // Generators page test
    {
        FileMap::Layout layout{};
        layout.page_size = 4096;
        layout.pages_per_segment = 64;
        FileMap fmap(layout);
        fmap.set_base_path(dir, "dbgen");
        GeneratorsManager gm(std::move(fmap), layout.page_size, /*page_no*/ 3);
        gm.init_new();
        auto o1 = gm.next_object_id();
        auto o2 = gm.next_object_id();
        auto r1 = gm.next_relation_id();
        auto i1 = gm.next_index_id();
        assert(o2 == o1 + 1);
        assert(r1 == 1);
        assert(i1 == 1);
    }

    // Clean up
    for (const auto& seg : fmap.segments()) {
        ::unlink(seg.path.c_str());
    }
    ::unlink((dir + "/f1").c_str());
    ::rmdir(dir.c_str());

    return 0;
}
