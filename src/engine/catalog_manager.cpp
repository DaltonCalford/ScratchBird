#include "scratchbird/engine/catalog_manager.h"

#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/header.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/index_btree.h"
#include "scratchbird/engine/uuid.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace scratchbird::engine
{

    CatalogManager::CatalogManager(std::string db_path) : db_path_(std::move(db_path)) {}

    bool CatalogManager::is_bootstrapped() const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(ps, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        // scan clumplets for CatalogVersion
        const std::uint8_t* p = buf.data() + sizeof(ods::PageHeader);
        const std::uint8_t* e = buf.data() + buf.size();
        while (p + 2 <= e) {
            auto t = static_cast<ClumpType>(*p++);
            if (t == ClumpType::End)
                break;
            std::uint8_t len = *p++;
            if (p + len > e)
                break;
            if (t == ClumpType::CatalogVersion && len >= 4) {
                return true;
            }
            p += len;
        }
        return false;
    }

    CatalogVersion CatalogManager::current_version() const
    {
        CatalogVersion out{};
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(ps, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        const std::uint8_t* p = buf.data() + sizeof(ods::PageHeader);
        const std::uint8_t* e = buf.data() + buf.size();
        while (p + 2 <= e) {
            auto t = static_cast<ClumpType>(*p++);
            if (t == ClumpType::End)
                break;
            std::uint8_t len = *p++;
            if (p + len > e)
                break;
            if (t == ClumpType::CatalogVersion && len >= 4) {
                std::uint16_t maj = 0, min = 0;
                std::memcpy(&maj, p, 2);
                std::memcpy(&min, p + 2, 2);
                out.major = maj;
                out.minor = min;
                return out;
            }
            p += len;
        }
        return out;
    }

    void CatalogManager::bootstrap_if_needed() const
    {
        // If header already has catalog version and catalog roots, assume done
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(ps, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);

        // Build a FileMap for writes if needed
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        FileMap fmap(layout);
        // split db_path_ to dir/base
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        fmap.set_base_path(dir, base);

        // Read header via HeaderManager to inspect existing roots
        HeaderManager hm(FileMap(layout), ps);
        hm = HeaderManager(std::move(fmap), ps);
        auto hi = hm.read();

        bool changed = false;
        if (!(hi.catalog_major && hi.catalog_minor)) {
            hi.catalog_major = 1;
            hi.catalog_minor = 0;
            changed = true;
        }
        // Ensure SDB$SOURCE root exists to hold view definitions
        if (!hi.sdb_source_root_page) {
            TupleLayout source_layout{};
            source_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, true}};
            FileMap fmap_s(layout);
            fmap_s.set_base_path(dir, base);
            Allocator alloc_s(&fmap_s, ps);
            std::uint32_t root_p = alloc_s.allocate_free_page();
            std::uint32_t data_p = alloc_s.allocate_free_page();
            std::vector<std::uint8_t> root(ps, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(root.data());
            ph->page_size = ps;
            ph->page_no = root_p;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_p;
            hr.last_heap_page = data_p;
            hr.tuple_format_id = compute_layout_format_id(source_layout);
            std::memcpy(root.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            ph->checksum = 0;
            ph->checksum = ods::crc32c(root.data(), root.size());
            fmap_s.write_page(root_p, root.data());
            std::vector<std::uint8_t> page(ps, 0);
            auto* ph2 = reinterpret_cast<ods::PageHeader*>(page.data());
            ph2->page_size = ps;
            ph2->page_no = data_p;
            ph2->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(page);
            ph2->checksum = 0;
            ph2->checksum = ods::crc32c(page.data(), page.size());
            fmap_s.write_page(data_p, page.data());
            hi.sdb_source_root_page = root_p;
            changed = true;
        }
        // Ensure SDB$SCHEMA root exists and seed 'public' schema deterministically
        if (!hi.sdb_schema_root_page) {
            TupleLayout schema_layout_tmp{};
            schema_layout_tmp.attrs = {
                {AttrType::VarBytes, 0, false, false}, // oid
                {AttrType::VarBytes, 0, false, true},  // parent_oid
                {AttrType::VarBytes, 0, false, false}, // name
                {AttrType::VarBytes, 0, false, false}, // kind
                {AttrType::VarBytes, 0, false, true}   // path_cache
            };
            FileMap fmap_s(layout);
            fmap_s.set_base_path(dir, base);
            Allocator alloc_s(&fmap_s, ps);
            std::uint32_t root_p = alloc_s.allocate_free_page();
            std::uint32_t data_p = alloc_s.allocate_free_page();
            std::vector<std::uint8_t> root(ps, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(root.data());
            ph->page_size = ps;
            ph->page_no = root_p;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_p;
            hr.last_heap_page = data_p;
            hr.tuple_format_id = compute_layout_format_id(schema_layout_tmp);
            std::memcpy(root.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            ph->checksum = 0;
            ph->checksum = ods::crc32c(root.data(), root.size());
            FileMap fmap_w(layout);
            fmap_w.set_base_path(dir, base);
            fmap_w.write_page(root_p, root.data());
            std::vector<std::uint8_t> page(ps, 0);
            auto* ph2 = reinterpret_cast<ods::PageHeader*>(page.data());
            ph2->page_size = ps;
            ph2->page_no = data_p;
            ph2->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(page);
            ph2->checksum = 0;
            ph2->checksum = ods::crc32c(page.data(), page.size());
            fmap_w.write_page(data_p, page.data());
            hi.sdb_schema_root_page = root_p;
            changed = true;
        }
        // Seed '<root>' and children when not present
        if (hi.sdb_schema_root_page) {
            TupleLayout schema_layout{};
            schema_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, true},
                                   {AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, true}};
            FileMap fmap_s(layout);
            fmap_s.set_base_path(dir, base);
            auto schema_rel =
                HeapRelation::open(std::move(fmap_s), ps, *hi.sdb_schema_root_page, schema_layout);
            bool has_root = false, has_public = false;
            {
                auto sc = schema_rel.open_scan();
                std::vector<Value> row;
                ods::RowId rid{};
                while (sc.next(row, &rid)) {
                    if (row.size() >= 4 && !row[2].is_null) {
                        if (row[2].bytes == std::string("<root>"))
                            has_root = true;
                        if (row[2].bytes == std::string("public"))
                            has_public = true;
                    }
                }
            }
            auto make_uuid = [](const UuidBytes& u) {
                Value v{};
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
                return v;
            };
            auto make_uuid_opt = [](const UuidBytes& u) {
                Value v{};
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
                return v;
            };
            auto make_null = []() -> Value {
                Value v{};
                v.is_null = true;
                return v;
            };
            auto make_str = [](const std::string& s) {
                Value v{};
                v.is_null = false;
                v.bytes = s;
                return v;
            };
            if (!has_root) {
                schema_rel.insert({make_uuid(oid_root_schema()), make_null(), make_str("<root>"),
                                   make_str("SYSTEM"), make_null()});
                changed = true;
            }
            if (!has_public) {
                schema_rel.insert({make_uuid(oid_public_schema()), make_uuid_opt(oid_root_schema()),
                                   make_str("public"), make_str("USER"), make_null()});
                changed = true;
            }
        }
        // Backfill: ensure all UNIQUE/PRIMARY KEY indexes have a physical B-Tree and stored root
        {
            TupleLayout object_layout{};
            object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, true},
                                   {AttrType::VarBytes, 0, false, false}};
            TupleLayout index_layout{};
            index_layout.attrs = {
                {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true},
                {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
                {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
                {AttrType::VarBytes, 0, false, true}};
            if (hi.sdb_object_root_page && hi.sdb_relation_root_page) {
                FileMap fmap_o2(layout);
                fmap_o2.set_base_path(dir, base);
                FileMap fmap_i2(layout);
                fmap_i2.set_base_path(dir, base);
                auto obj_rel = HeapRelation::open(std::move(fmap_o2), ps, *hi.sdb_object_root_page,
                                                  object_layout);
                auto idx_rel = HeapRelation::open(std::move(fmap_i2), ps,
                                                  *hi.sdb_relation_root_page, index_layout);
                struct TmpIdx {
                    std::string name;
                    bool unique{false};
                    std::string method;
                };
                std::unordered_map<std::string, TmpIdx> idxs;
                {
                    auto sc = obj_rel.open_scan();
                    std::vector<Value> row;
                    ods::RowId rid{};
                    while (sc.next(row, &rid)) {
                        if (row.size() < 4)
                            continue;
                        if (row[1].is_null || row[1].bytes != std::string("INDEX"))
                            continue;
                        if (row[0].is_null || row[0].bytes.size() != 16)
                            continue;
                        std::string oid(row[0].bytes.data(), row[0].bytes.size());
                        TmpIdx t{};
                        t.name = row[3].is_null ? std::string() : row[3].bytes;
                        idxs.emplace(std::move(oid), std::move(t));
                    }
                }
                if (!idxs.empty()) {
                    auto sc = idx_rel.open_scan();
                    std::vector<Value> row;
                    ods::RowId rid{};
                    while (sc.next(row, &rid)) {
                        if (row.size() < 4)
                            continue;
                        if (row[0].is_null || row[0].bytes.size() != 16)
                            continue;
                        std::string oid(row[0].bytes.data(), row[0].bytes.size());
                        auto it = idxs.find(oid);
                        if (it == idxs.end())
                            continue;
                        it->second.method = row[3].is_null ? std::string() : row[3].bytes;
                        std::string uniq = row[2].is_null ? std::string() : row[2].bytes;
                        it->second.unique = (uniq == "TRUE");
                    }
                    for (const auto& [oid, info] : idxs) {
                        if (!info.unique)
                            continue;
                        if (info.method != "BTREE")
                            continue;
                        if (info.name.empty())
                            continue;
                        auto root_opt = const_cast<CatalogManager*>(this)->get_index_root(
                            std::nullopt, info.name);
                        if (!root_opt) {
                            try {
                                FileMap fm_idx(layout);
                                fm_idx.set_base_path(dir, base);
                                BTreeIndex bt(std::move(fm_idx), ps, /*unique*/ true);
                                bt.create_empty();
                                const_cast<CatalogManager*>(this)->set_index_root(
                                    std::nullopt, info.name, bt.root_page());
                                changed = true;
                            } catch (...) {
                            }
                        }
                    }
                }
            }
        }

        if (changed)
            hm.write_new(hi);
    }

    bool CatalogManager::create_schema(const UuidBytes& oid, const std::string& name,
                                       const std::optional<UuidBytes>& parent,
                                       const std::string& kind) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        // Build FileMap
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        FileMap fmap(layout);
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        fmap.set_base_path(dir, base);

        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_schema_root_page) {
            // Allocate SDB$SCHEMA heap root if missing
            // Build layout matching (oid, parent_oid, name, kind, path_cache)
            TupleLayout schema_layout_tmp{};
            schema_layout_tmp.attrs = {{AttrType::VarBytes, 0, false, false},
                                       {AttrType::VarBytes, 0, false, true},
                                       {AttrType::VarBytes, 0, false, false},
                                       {AttrType::VarBytes, 0, false, false},
                                       {AttrType::VarBytes, 0, false, true}};
            FileMap fmap_alloc(layout);
            fmap_alloc.set_base_path(dir, base);
            Allocator alloc(&fmap_alloc, ps);
            std::uint32_t root_p = alloc.allocate_free_page();
            std::uint32_t data_p = alloc.allocate_free_page();
            std::vector<std::uint8_t> rootpg(ps, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(rootpg.data());
            ph->page_size = ps;
            ph->page_no = root_p;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_p;
            hr.last_heap_page = data_p;
            hr.tuple_format_id = compute_layout_format_id(schema_layout_tmp);
            std::memcpy(rootpg.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            ph->checksum = 0;
            ph->checksum = ods::crc32c(rootpg.data(), rootpg.size());
            FileMap fmap_w(layout);
            fmap_w.set_base_path(dir, base);
            fmap_w.write_page(root_p, rootpg.data());
            // Initialize first data page
            std::vector<std::uint8_t> datapg(ps, 0);
            auto* pd = reinterpret_cast<ods::PageHeader*>(datapg.data());
            pd->page_size = ps;
            pd->page_no = data_p;
            pd->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            // Initialize as empty heap data page payload footprint (reuse root payload struct size
            // for checksum)
            std::memset(datapg.data() + sizeof(ods::PageHeader), 0, sizeof(ods::HeapRootPayload));
            pd->checksum = 0;
            pd->checksum = ods::crc32c(datapg.data(), datapg.size());
            fmap_w.write_page(data_p, datapg.data());
            hi.sdb_schema_root_page = root_p;
            hm.write_new(hi);
        }
        // Open schema heap
        // TupleLayout must match engine bootstrap (oid, parent_oid, name, kind, path_cache)
        TupleLayout schema_layout{};
        schema_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, // oid
            {AttrType::VarBytes, 0, false, true},  // parent_oid
            {AttrType::VarBytes, 0, false, false}, // name
            {AttrType::VarBytes, 0, false, false}, // kind
            {AttrType::VarBytes, 0, false, true}   // path_cache
        };
        // Idempotency: if a schema with this name already exists, do nothing
        {
            FileMap fmap_scan(layout);
            fmap_scan.set_base_path(dir, base);
            HeapRelation scan_rel = HeapRelation::open(std::move(fmap_scan), ps,
                                                       *hi.sdb_schema_root_page, schema_layout);
            auto sc = scan_rel.open_scan();
            std::vector<Value> row;
            while (sc.next(row, nullptr)) {
                if (row.size() < 4)
                    continue;
                if (!row[2].is_null && row[2].bytes == name) {
                    // already present
                    return true;
                }
            }
        }
        // Prepare values
        auto make_uuid_val = [](const UuidBytes& u) {
            Value v{};
            v.is_null = false;
            v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
            return v;
        };
        auto make_opt_uuid_val = [](const std::optional<UuidBytes>& u) {
            Value v{};
            if (u.has_value()) {
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u->data()), u->size());
            } else {
                v.is_null = true;
            }
            return v;
        };
        auto make_str = [](const std::string& s) {
            Value v{};
            v.is_null = false;
            v.bytes = s;
            return v;
        };
        // Use a fresh FileMap because hm consumed the previous one
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);
        HeapRelation rel =
            HeapRelation::open(std::move(fmap2), ps, *hi.sdb_schema_root_page, schema_layout);
        // Insert with NULL path_cache initially
        Value vnull{};
        vnull.is_null = true;
        rel.insert(
            {make_uuid_val(oid), make_opt_uuid_val(parent), make_str(name), make_str(kind), vnull});
        return true;
    }

    std::optional<UuidBytes>
    CatalogManager::lookup_schema_oid_by_name(const std::string& name) const
    {
        std::fprintf(stderr, "[CAT] lookup_schema_oid_by_name name='%s' db='%s'\n", name.c_str(),
                     db_path_.c_str());
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_schema_root_page)
            return std::nullopt;
        else
            std::fprintf(stderr, "[CAT] schema_root_page=%u\n", *hi.sdb_schema_root_page);
        TupleLayout schema_layout{};
        schema_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true}};
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);
        HeapRelation rel =
            HeapRelation::open(std::move(fmap2), ps, *hi.sdb_schema_root_page, schema_layout);
        // Sequential scan
        auto scan = rel.open_scan();
        std::vector<Value> out;
        ods::RowId rid{};
        while (scan.next(out, &rid)) {
            if (out.size() < 4)
                continue;
            if (!out[2].is_null && out[2].bytes == name) {
                std::fprintf(stderr, "[CAT] matched schema row name='%s'\n", out[2].bytes.c_str());
                UuidBytes u{};
                if (!out[0].is_null && out[0].bytes.size() == u.size()) {
                    std::memcpy(u.data(), out[0].bytes.data(), u.size());
                    return u;
                }
            }
        }
        // Fallback: ensure default schema exists
        if (name == std::string("public")) {
            // Create and return 'public' schema on-demand with well-known OID
            std::fprintf(stderr, "[CAT] fallback create schema 'public' (well-known OID)\n");
            UuidBytes gen = engine::oid_public_schema();
            const_cast<CatalogManager*>(this)->create_schema(gen, name, std::nullopt,
                                                             std::string("USER"));
            // Re-scan quickly to return it
            auto sc2 = rel.open_scan();
            out.clear();
            rid = {};
            while (sc2.next(out, &rid)) {
                if (out.size() < 4)
                    continue;
                if (!out[2].is_null && out[2].bytes == name) {
                    UuidBytes u{};
                    if (!out[0].is_null && out[0].bytes.size() == u.size()) {
                        std::memcpy(u.data(), out[0].bytes.data(), u.size());
                        return u;
                    }
                }
            }
        }
        return std::nullopt;
    }

    std::vector<std::pair<UuidBytes, std::string>> CatalogManager::list_schemas() const
    {
        std::vector<std::pair<UuidBytes, std::string>> result;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_schema_root_page)
            return result;
        TupleLayout schema_layout{};
        schema_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true}};
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);
        HeapRelation rel =
            HeapRelation::open(std::move(fmap2), ps, *hi.sdb_schema_root_page, schema_layout);
        auto scan = rel.open_scan();
        std::vector<Value> out;
        ods::RowId rid{};
        while (scan.next(out, &rid)) {
            if (out.size() < 4)
                continue;
            UuidBytes u{};
            if (!out[0].is_null && out[0].bytes.size() == u.size()) {
                std::memcpy(u.data(), out[0].bytes.data(), u.size());
                std::string nm = out[2].is_null ? std::string() : out[2].bytes;
                result.emplace_back(u, nm);
            }
        }
        return result;
    }

    bool CatalogManager::create_object(const UuidBytes& oid, const std::string& type,
                                       const std::optional<UuidBytes>& schema_oid,
                                       const std::string& name) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page)
            return false;
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        auto make_uuid_val = [](const UuidBytes& u) {
            Value v{};
            v.is_null = false;
            v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
            return v;
        };
        auto make_opt_uuid_val = [](const std::optional<UuidBytes>& u) {
            Value v{};
            if (u.has_value()) {
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u->data()), u->size());
            } else {
                v.is_null = true;
            }
            return v;
        };
        auto make_str = [](const std::string& s) {
            Value v{};
            v.is_null = false;
            v.bytes = s;
            return v;
        };
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);
        HeapRelation rel =
            HeapRelation::open(std::move(fmap2), ps, *hi.sdb_object_root_page, object_layout);
        rel.insert(
            {make_uuid_val(oid), make_str(type), make_opt_uuid_val(schema_oid), make_str(name)});
        return true;
    }

    std::optional<UuidBytes>
    CatalogManager::lookup_object_oid(const std::optional<UuidBytes>& schema_oid,
                                      const std::string& type, const std::string& name) const
    {
        std::fprintf(stderr, "[CAT] lookup_object_oid type='%s' name='%s' schema=%s db='%s'\n",
                     type.c_str(), name.c_str(), schema_oid ? "set" : "<null>", db_path_.c_str());
        auto to_upper = [](const std::string& s) {
            std::string r = s;
            for (char& c : r)
                c = (char)std::toupper((unsigned char)c);
            return r;
        };
        auto to_lower = [](const std::string& s) {
            std::string r = s;
            for (char& c : r)
                c = (char)std::tolower((unsigned char)c);
            return r;
        };
        auto trim = [](const std::string& s) {
            size_t a = 0;
            while (a < s.size() && std::isspace((unsigned char)s[a]))
                ++a;
            size_t b = s.size();
            while (b > a && std::isspace((unsigned char)s[b - 1]))
                --b;
            return s.substr(a, b - a);
        };
        const std::string type_norm = to_upper(type);
        const std::string name_norm_lower = to_lower(trim(name));
        if (schema_oid) {
            static const char* hx = "0123456789abcdef";
            std::string s;
            s.resize(schema_oid->size() * 2);
            for (size_t i = 0; i < schema_oid->size(); ++i) {
                unsigned char c = (unsigned char)(*schema_oid)[i];
                s[i * 2] = hx[(c >> 4) & 0xF];
                s[i * 2 + 1] = hx[c & 0xF];
            }
            std::fprintf(stderr, "[CAT] expected schema_oid=%s\n", s.c_str());
        }
        auto hex16 = [](const std::string& b) {
            static const char* hx = "0123456789abcdef";
            std::string s;
            s.resize(b.size() * 2);
            for (size_t i = 0; i < b.size(); ++i) {
                unsigned char c = (unsigned char)b[i];
                s[i * 2] = hx[(c >> 4) & 0xF];
                s[i * 2 + 1] = hx[c & 0xF];
            }
            return s;
        };
        auto hex16a = [](const UuidBytes& u) {
            static const char* hx = "0123456789abcdef";
            std::string s;
            s.resize(u.size() * 2);
            for (size_t i = 0; i < u.size(); ++i) {
                unsigned char c = (unsigned char)u[i];
                s[i * 2] = hx[(c >> 4) & 0xF];
                s[i * 2 + 1] = hx[c & 0xF];
            }
            return s;
        };
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page)
            return std::nullopt;
        else
            std::fprintf(stderr, "[CAT] OBJECT root at %u\n", *hi.sdb_object_root_page);
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);
        HeapRelation rel =
            HeapRelation::open(std::move(fmap2), ps, *hi.sdb_object_root_page, object_layout);
        auto scan = rel.open_scan();
        std::vector<Value> out;
        ods::RowId rid{};
        std::size_t scanned_rows = 0;
        std::size_t schema_hits = 0;
        while (scan.next(out, &rid)) {
            std::fprintf(stderr, "[CAT] OBJECT row tuple_size=%zu\n", out.size());
            ++scanned_rows;
            if (out.size() != 4)
                continue;
            bool schema_match = false;
            if (!schema_oid.has_value()) {
                schema_match = out[2].is_null;
            } else {
                if (out[2].is_null)
                    continue;
                if (out[2].bytes.size() != schema_oid->size())
                    continue;
                schema_match =
                    std::memcmp(out[2].bytes.data(), schema_oid->data(), schema_oid->size()) == 0;
            }
            if (!out[1].is_null && !out[3].is_null) {
                std::fprintf(
                    stderr,
                    "[CAT] OBJECT row type='%s' name='%s' schema_match=%d actual=%s expected=%s\n",
                    out[1].bytes.c_str(), out[3].bytes.c_str(), (int)schema_match,
                    out[2].is_null ? "<null>" : hex16(out[2].bytes).c_str(),
                    schema_oid ? hex16a(*schema_oid).c_str() : "<null>");
            }
            if (!schema_match)
                continue;
            ++schema_hits;
            if (!out[1].is_null && !out[3].is_null) {
                const std::string row_type_upper = to_upper(out[1].bytes);
                const std::string row_name_trim_lower = to_lower(trim(out[3].bytes));
                if (row_type_upper != type_norm || row_name_trim_lower != name_norm_lower) {
                    std::fprintf(stderr,
                                 "[CAT] OBJECT row mismatch type/name: row_type='%s' row_name='%s' "
                                 "vs expected type='%s' name='%s' (normalized)\n",
                                 row_type_upper.c_str(), row_name_trim_lower.c_str(),
                                 type_norm.c_str(), name_norm_lower.c_str());
                }
                if (row_type_upper == type_norm && row_name_trim_lower == name_norm_lower) {
                    UuidBytes u{};
                    if (!out[0].is_null && out[0].bytes.size() == u.size()) {
                        std::memcpy(u.data(), out[0].bytes.data(), u.size());
                        std::fprintf(stderr, "[CAT] lookup_object_oid FOUND type='%s' name='%s'\n",
                                     type.c_str(), name.c_str());
                        return u;
                    }
                }
            }
        }
        std::fprintf(stderr, "[CAT] lookup_object_oid scanned=%zu schema_hits=%zu\n", scanned_rows,
                     schema_hits);
        std::fprintf(stderr, "[CAT] lookup_object_oid NOT FOUND type='%s' name='%s' schema=%s\n",
                     type.c_str(), name.c_str(), schema_oid ? "set" : "<null>");
        return std::nullopt;
    }

    std::vector<std::tuple<UuidBytes, std::string, std::string>>
    CatalogManager::list_objects_in_schema(const std::optional<UuidBytes>& schema_oid) const
    {
        std::vector<std::tuple<UuidBytes, std::string, std::string>> result;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page)
            return result;
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);
        HeapRelation rel =
            HeapRelation::open(std::move(fmap2), ps, *hi.sdb_object_root_page, object_layout);
        auto scan = rel.open_scan();
        std::vector<Value> out;
        ods::RowId rid{};
        while (scan.next(out, &rid)) {
            if (out.size() != 4)
                continue;
            bool schema_match = false;
            if (!schema_oid.has_value()) {
                schema_match = out[2].is_null;
            } else {
                if (out[2].is_null)
                    continue;
                if (out[2].bytes.size() != schema_oid->size())
                    continue;
                schema_match =
                    std::memcmp(out[2].bytes.data(), schema_oid->data(), schema_oid->size()) == 0;
            }
            if (!schema_match)
                continue;
            UuidBytes u{};
            if (!out[0].is_null && out[0].bytes.size() == u.size()) {
                std::memcpy(u.data(), out[0].bytes.data(), u.size());
                std::string type = out[1].is_null ? std::string() : out[1].bytes;
                std::string name = out[3].is_null ? std::string() : out[3].bytes;
                result.emplace_back(u, std::move(type), std::move(name));
            }
        }
        return result;
    }

    UuidBytes CatalogManager::create_relation(const UuidBytes& schema_oid, const std::string& name,
                                              const std::vector<std::string>& column_names) const
    {
        std::fprintf(stderr, "[CAT] create_relation schema_oid(set) name='%s' cols=%zu db='%s'\n",
                     name.c_str(), column_names.size(), db_path_.c_str());
        // Allocate relation OID by hashing (temporary)
        UuidBytes oid{};
        {
            std::hash<std::string> h;
            auto v =
                h(std::string(reinterpret_cast<const char*>(schema_oid.data()), schema_oid.size()) +
                  std::string("::") + name);
            std::memcpy(oid.data(), &v, std::min(sizeof(v), oid.size()));
        }
        // Open header and file map
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        // Insert into SDB$OBJECT (type=RELATION) and SDB$RELATION
        // Ensure OBJECT root exists; if missing (older headers), create it now
        if (!hi.sdb_object_root_page) {
            std::fprintf(stderr, "[CAT] create_relation: OBJECT root missing; creating now\n");
            // Create OBJECT heap with layout: oid, type, schema_oid, name (all VarBytes; schema_oid
            // nullable)
            TupleLayout object_layout{};
            object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, true},
                                   {AttrType::VarBytes, 0, false, false}};
            FileMap fmap_alloc(layout);
            fmap_alloc.set_base_path(dir, base);
            Allocator alloc_obj(&fmap_alloc, ps);
            std::uint32_t root_p = alloc_obj.allocate_free_page();
            std::uint32_t data_p = alloc_obj.allocate_free_page();
            std::vector<std::uint8_t> rootpg(ps, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(rootpg.data());
            ph->page_size = ps;
            ph->page_no = root_p;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_p;
            hr.last_heap_page = data_p;
            hr.tuple_format_id = compute_layout_format_id(object_layout);
            std::memcpy(rootpg.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            ph->checksum = 0;
            ph->checksum = ods::crc32c(rootpg.data(), rootpg.size());
            FileMap fmap_w(layout);
            fmap_w.set_base_path(dir, base);
            fmap_w.write_page(root_p, rootpg.data());
            std::vector<std::uint8_t> datapg(ps, 0);
            auto* ph2 = reinterpret_cast<ods::PageHeader*>(datapg.data());
            ph2->page_size = ps;
            ph2->page_no = data_p;
            ph2->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(datapg);
            ph2->checksum = 0;
            ph2->checksum = ods::crc32c(datapg.data(), datapg.size());
            fmap_w.write_page(data_p, datapg.data());
            // Persist to header
            hi.sdb_object_root_page = root_p;
            HeaderManager hm_sync(FileMap(layout), ps);
            {
                FileMap fmap_sync(layout);
                fmap_sync.set_base_path(dir, base);
                hm_sync = HeaderManager(std::move(fmap_sync), ps);
            }
            hm_sync.write_new(hi);
            std::fprintf(stderr, "[CAT] create_relation: OBJECT root created at %u\n", root_p);
        }
        // OBJECT
        if (hi.sdb_object_root_page) {
            TupleLayout object_layout{};
            object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, true},
                                   {AttrType::VarBytes, 0, false, false}};
            FileMap fmap_o(layout);
            fmap_o.set_base_path(dir, base);
            HeapRelation object_rel =
                HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, object_layout);
            auto make_uuid_val = [](const UuidBytes& u) {
                Value v{};
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
                return v;
            };
            auto make_opt_uuid_val = [](const std::optional<UuidBytes>& u) {
                Value v{};
                if (u.has_value()) {
                    v.is_null = false;
                    v.bytes.assign(reinterpret_cast<const char*>(u->data()), u->size());
                } else {
                    v.is_null = true;
                }
                return v;
            };
            auto make_str = [](const std::string& s) {
                Value v{};
                v.is_null = false;
                v.bytes = s;
                return v;
            };
            object_rel.insert({make_uuid_val(oid), make_str("RELATION"), make_uuid_val(schema_oid),
                               make_str(name)});
            auto hex16a = [](const UuidBytes& u) {
                static const char* hx = "0123456789abcdef";
                std::string s;
                s.resize(u.size() * 2);
                for (size_t i = 0; i < u.size(); ++i) {
                    unsigned char c = (unsigned char)u[i];
                    s[i * 2] = hx[(c >> 4) & 0xF];
                    s[i * 2 + 1] = hx[c & 0xF];
                }
                return s;
            };
            std::fprintf(
                stderr,
                "[CAT] create_relation OBJECT row: schema_oid=%s name='%s' type='RELATION'\n",
                hex16a(schema_oid).c_str(), name.c_str());
            std::fprintf(stderr, "[CAT] create_relation wrote OBJECT row name='%s'\n",
                         name.c_str());
        }
        // Ensure RELATION root exists; if missing (older headers), create it now
        if (!hi.sdb_relation_root_page) {
            std::fprintf(stderr, "[CAT] create_relation: RELATION root missing; creating now\n");
            TupleLayout relation_layout{};
            relation_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                     {AttrType::Int64, 8, true, false}};
            FileMap fmap_alloc(layout);
            fmap_alloc.set_base_path(dir, base);
            Allocator alloc_rel(&fmap_alloc, ps);
            std::uint32_t root_p = alloc_rel.allocate_free_page();
            std::uint32_t data_p = alloc_rel.allocate_free_page();
            std::vector<std::uint8_t> rootpg(ps, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(rootpg.data());
            ph->page_size = ps;
            ph->page_no = root_p;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_p;
            hr.last_heap_page = data_p;
            hr.tuple_format_id = compute_layout_format_id(relation_layout);
            std::memcpy(rootpg.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            ph->checksum = 0;
            ph->checksum = ods::crc32c(rootpg.data(), rootpg.size());
            FileMap fmap_w(layout);
            fmap_w.set_base_path(dir, base);
            fmap_w.write_page(root_p, rootpg.data());
            std::vector<std::uint8_t> datapg(ps, 0);
            auto* ph2 = reinterpret_cast<ods::PageHeader*>(datapg.data());
            ph2->page_size = ps;
            ph2->page_no = data_p;
            ph2->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(datapg);
            ph2->checksum = 0;
            ph2->checksum = ods::crc32c(datapg.data(), datapg.size());
            fmap_w.write_page(data_p, datapg.data());
            hi.sdb_relation_root_page = root_p;
            HeaderManager hm_sync(FileMap(layout), ps);
            {
                FileMap fmap_sync(layout);
                fmap_sync.set_base_path(dir, base);
                hm_sync = HeaderManager(std::move(fmap_sync), ps);
            }
            hm_sync.write_new(hi);
            std::fprintf(stderr, "[CAT] create_relation: RELATION root created at %u\n", root_p);
        }
        // RELATION: allocate heap storage root + first data page now and persist root page id
        if (hi.sdb_relation_root_page) {
            TupleLayout relation_layout{};
            relation_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                     {AttrType::Int64, 8, true, false}};
            FileMap fmap_r(layout);
            fmap_r.set_base_path(dir, base);
            HeapRelation rel_rel = HeapRelation::open(std::move(fmap_r), ps,
                                                      *hi.sdb_relation_root_page, relation_layout);
            // Allocate heap root + data page for the relation
            FileMap fmap_alloc(layout);
            fmap_alloc.set_base_path(dir, base);
            Allocator alloc(&fmap_alloc, ps);
            std::uint32_t root_p = alloc.allocate_free_page();
            std::uint32_t data_p = alloc.allocate_free_page();
            // Initialize pages
            std::vector<std::uint8_t> rootpg(ps, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(rootpg.data());
            ph->page_size = ps;
            ph->page_no = root_p;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_p;
            hr.last_heap_page = data_p;
            // Build a VarBytes-only layout id for now (one per column)
            TupleLayout row_layout{};
            for (const auto& c : column_names) {
                (void)c;
                row_layout.attrs.push_back({AttrType::VarBytes, 0, false, true});
            }
            hr.tuple_format_id = compute_layout_format_id(row_layout);
            std::memcpy(rootpg.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            ph->checksum = 0;
            ph->checksum = ods::crc32c(rootpg.data(), rootpg.size());
            FileMap fmap_w(layout);
            fmap_w.set_base_path(dir, base);
            fmap_w.write_page(root_p, rootpg.data());
            std::vector<std::uint8_t> datapg(ps, 0);
            auto* ph2 = reinterpret_cast<ods::PageHeader*>(datapg.data());
            ph2->page_size = ps;
            ph2->page_no = data_p;
            ph2->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(datapg);
            ph2->checksum = 0;
            ph2->checksum = ods::crc32c(datapg.data(), datapg.size());
            fmap_w.write_page(data_p, datapg.data());

            Value oidv{};
            oidv.is_null = false;
            oidv.bytes.assign(reinterpret_cast<const char*>(oid.data()), oid.size());
            Value rootv{};
            rootv.is_null = false;
            rootv.u64 = root_p;
            rel_rel.insert({oidv, rootv});
            std::fprintf(stderr, "[CAT] create_relation wrote RELATION root=%u name='%s'\n", root_p,
                         name.c_str());
            // Verify mapping visible immediately
            {
                auto sc = rel_rel.open_scan();
                std::vector<Value> rv;
                bool seen = false;
                std::uint64_t got_root = 0;
                while (sc.next(rv, nullptr)) {
                    if (rv.size() < 2)
                        continue;
                    if (rv[0].is_null || rv[0].bytes.size() != 16)
                        continue;
                    if (std::memcmp(rv[0].bytes.data(), oid.data(), 16) != 0)
                        continue;
                    seen = true;
                    got_root = rv[1].is_null ? 0ull : rv[1].u64;
                    break;
                }
                std::fprintf(stderr,
                             "[CAT] create_relation: REL map visible=%s root=%llu for '%s'\n",
                             seen ? "yes" : "no", (unsigned long long)got_root, name.c_str());
            }
            // Ensure header clumplets are flushed with latest roots
            HeaderManager hm_sync(FileMap(layout), ps);
            {
                FileMap fmap_sync(layout);
                fmap_sync.set_base_path(dir, base);
                hm_sync = HeaderManager(std::move(fmap_sync), ps);
            }
            auto hi_sync = hm_sync.read();
            hm_sync.write_new(hi_sync);
        }
        // Ensure COLUMN root exists; if missing (older headers), create it now
        if (!hi.sdb_column_root_page) {
            std::fprintf(stderr, "[CAT] create_relation: COLUMN root missing; creating now\n");
            TupleLayout column_layout{};
            column_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::Int64, 8, true, false},
                                   {AttrType::VarBytes, 0, false, false}};
            FileMap fmap_alloc(layout);
            fmap_alloc.set_base_path(dir, base);
            Allocator alloc_col(&fmap_alloc, ps);
            std::uint32_t root_p = alloc_col.allocate_free_page();
            std::uint32_t data_p = alloc_col.allocate_free_page();
            std::vector<std::uint8_t> rootpg(ps, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(rootpg.data());
            ph->page_size = ps;
            ph->page_no = root_p;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_p;
            hr.last_heap_page = data_p;
            hr.tuple_format_id = compute_layout_format_id(column_layout);
            std::memcpy(rootpg.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            ph->checksum = 0;
            ph->checksum = ods::crc32c(rootpg.data(), rootpg.size());
            FileMap fmap_w(layout);
            fmap_w.set_base_path(dir, base);
            fmap_w.write_page(root_p, rootpg.data());
            std::vector<std::uint8_t> datapg(ps, 0);
            auto* ph2 = reinterpret_cast<ods::PageHeader*>(datapg.data());
            ph2->page_size = ps;
            ph2->page_no = data_p;
            ph2->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(datapg);
            ph2->checksum = 0;
            ph2->checksum = ods::crc32c(datapg.data(), datapg.size());
            fmap_w.write_page(data_p, datapg.data());
            hi.sdb_column_root_page = root_p;
            HeaderManager hm_sync(FileMap(layout), ps);
            {
                FileMap fmap_sync(layout);
                fmap_sync.set_base_path(dir, base);
                hm_sync = HeaderManager(std::move(fmap_sync), ps);
            }
            hm_sync.write_new(hi);
            std::fprintf(stderr, "[CAT] create_relation: COLUMN root created at %u\n", root_p);
        }
        // COLUMNS
        if (hi.sdb_column_root_page) {
            TupleLayout column_layout{};
            column_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::Int64, 8, true, false},
                                   {AttrType::VarBytes, 0, false, false}};
            FileMap fmap_c(layout);
            fmap_c.set_base_path(dir, base);
            HeapRelation col_rel =
                HeapRelation::open(std::move(fmap_c), ps, *hi.sdb_column_root_page, column_layout);
            for (size_t i = 0; i < column_names.size(); ++i) {
                Value relv{};
                relv.is_null = false;
                relv.bytes.assign(reinterpret_cast<const char*>(oid.data()), oid.size());
                Value posv{};
                posv.is_null = false;
                posv.u64 = static_cast<std::uint64_t>(i + 1);
                Value namev{};
                namev.is_null = false;
                namev.bytes = column_names[i];
                col_rel.insert({relv, posv, namev});
            }
            std::fprintf(stderr, "[CAT] create_relation wrote %zu COLUMNS for name='%s'\n",
                         column_names.size(), name.c_str());
        }
        return oid;
    }

    std::vector<std::pair<UuidBytes, std::string>>
    CatalogManager::list_relations(const std::optional<UuidBytes>& schema_oid) const
    {
        std::vector<std::pair<UuidBytes, std::string>> out;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(ps, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        FileMap::Layout layout_h{};
        layout_h.page_size = ps;
        layout_h.pages_per_segment = 262144;
        FileMap fmap_h(layout_h);
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        fmap_h.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap_h), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page) {
            return out;
        }
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);

        // Read all OBJECT rows and filter type=RELATION (or TABLE/VIEW later)
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        auto object_rel =
            HeapRelation::open(std::move(fmap), ps, *hi.sdb_object_root_page, object_layout);
        auto scan = object_rel.open_scan();
        std::vector<Value> row;
        while (scan.next(row, nullptr)) {
            if (row.size() < 4)
                continue;
            const auto& type = row[1];
            if (type.is_null)
                continue;
            if (type.bytes != std::string("RELATION") && type.bytes != std::string("TABLE") &&
                type.bytes != std::string("VIEW"))
                continue;
            if (schema_oid.has_value()) {
                if (row[2].is_null)
                    continue;
                if (row[2].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[2].bytes.data(), schema_oid->data(), 16) != 0)
                    continue;
            }
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            UuidBytes roid{};
            std::memcpy(roid.data(), row[0].bytes.data(), 16);
            out.emplace_back(roid, row[3].bytes);
        }
        return out;
    }

    std::vector<std::pair<std::int64_t, std::string>>
    CatalogManager::list_columns(const UuidBytes& relation_oid) const
    {
        std::vector<std::pair<std::int64_t, std::string>> out;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(ps, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        FileMap::Layout layout_h2{};
        layout_h2.page_size = ps;
        layout_h2.pages_per_segment = 262144;
        FileMap fmap_h2(layout_h2);
        auto slash2 = db_path_.find_last_of('/');
        std::string dir2 =
            (slash2 == std::string::npos) ? std::string(".") : db_path_.substr(0, slash2);
        std::string base2 = (slash2 == std::string::npos) ? db_path_ : db_path_.substr(slash2 + 1);
        fmap_h2.set_base_path(dir2, base2);
        HeaderManager hm2(std::move(fmap_h2), ps);
        auto hi = hm2.read();
        if (!hi.sdb_column_root_page)
            return out;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        FileMap fmap(layout);
        auto slash3 = db_path_.find_last_of('/');
        std::string dir3 =
            (slash3 == std::string::npos) ? std::string(".") : db_path_.substr(0, slash3);
        std::string base3 = (slash3 == std::string::npos) ? db_path_ : db_path_.substr(slash3 + 1);
        fmap.set_base_path(dir3, base3);
        TupleLayout column_layout{};
        column_layout.attrs = {{AttrType::VarBytes, 0, false, false}, // 0: relation_oid
                               {AttrType::Int64, 8, true, false},     // 1: position
                               {AttrType::VarBytes, 0, false, false}, // 2: name
                               {AttrType::Int64, 8, true, false},     // 3: not_null (0/1)
                               {AttrType::VarBytes, 0, false, true},  // 4: domain_oid (nullable)
                               {AttrType::VarBytes, 0, false, true},  // 5: type_info (nullable)
                               {AttrType::VarBytes, 0, false, true}}; // 6: default_expr (nullable)
        auto col_rel =
            HeapRelation::open(std::move(fmap), ps, *hi.sdb_column_root_page, column_layout);
        auto scan = col_rel.open_scan();
        std::vector<Value> row;
        while (scan.next(row, nullptr)) {
            if (row.size() < 7)
                continue;
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            if (std::memcmp(row[0].bytes.data(), relation_oid.data(), 16) != 0)
                continue;
            std::int64_t pos = row[1].is_null ? 0 : row[1].u64;
            std::string name = row[2].is_null ? std::string() : row[2].bytes;
            out.emplace_back(pos, name);
        }
        std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.first < b.first; });
        return out;
    }

    bool CatalogManager::create_columns(
        const UuidBytes& relation_oid,
        const std::vector<std::pair<std::int64_t, std::string>>& columns,
        const std::vector<std::string>& not_null_columns,
        const std::unordered_map<std::string, std::string>& column_defaults) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_column_root_page)
            return false;
        TupleLayout col_layout{};
        col_layout.attrs = {{AttrType::VarBytes, 0, false, false}, // 0: relation_oid
                            {AttrType::Int64, 8, true, false},     // 1: position
                            {AttrType::VarBytes, 0, false, false}, // 2: name
                            {AttrType::Int64, 8, true, false},     // 3: not_null (0/1)
                            {AttrType::VarBytes, 0, false, true},  // 4: domain_oid (nullable)
                            {AttrType::VarBytes, 0, false, true},  // 5: type_info (nullable)
                            {AttrType::VarBytes, 0, false, true}}; // 6: default_expr (nullable)
        FileMap fmap_c(layout);
        fmap_c.set_base_path(dir, base);
        auto col_rel =
            HeapRelation::open(std::move(fmap_c), ps, *hi.sdb_column_root_page, col_layout);
        auto make_uuid_val = [](const UuidBytes& u) {
            Value v{};
            v.is_null = false;
            v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
            return v;
        };
        auto make_i64 = [](std::int64_t x) {
            Value v{};
            v.is_null = false;
            v.u64 = static_cast<std::uint64_t>(x);
            return v;
        };
        auto make_str = [](const std::string& s) {
            Value v{};
            v.is_null = false;
            v.bytes = s;
            return v;
        };
        std::unordered_set<std::string> nn(not_null_columns.begin(), not_null_columns.end());
        auto make_null = []() {
            Value v{};
            v.is_null = true;
            return v;
        };
        for (auto& [pos, name] : columns) {
            std::uint64_t notnull = nn.count(name) ? 1ull : 0ull;

            // Check if column has a default value
            Value default_val = make_null();
            auto it = column_defaults.find(name);
            if (it != column_defaults.end()) {
                default_val = make_str(it->second);
            }

            col_rel.insert({make_uuid_val(relation_oid), make_i64(pos), make_str(name),
                            make_i64((std::int64_t)notnull), make_null(), make_null(),
                            default_val});
        }
        return true;
    }

    std::optional<std::uint32_t>
    CatalogManager::get_relation_root_page(const UuidBytes& relation_oid) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(ps, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        FileMap fmap(layout);
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_relation_root_page)
            return std::nullopt;
        TupleLayout relation_layout{};
        relation_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                 {AttrType::Int64, 8, true, false}};
        FileMap fmap_r(layout);
        fmap_r.set_base_path(dir, base);
        auto rel_rel =
            HeapRelation::open(std::move(fmap_r), ps, *hi.sdb_relation_root_page, relation_layout);
        auto scan = rel_rel.open_scan();
        std::vector<Value> row;
        while (scan.next(row, nullptr)) {
            if (row.size() < 2)
                continue;
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            // Debug: dump each RELATION row oid/root
            {
                static const char* hx = "0123456789abcdef";
                std::string soid;
                soid.resize(32);
                for (size_t i = 0; i < 16; ++i) {
                    unsigned char c = (unsigned char)row[0].bytes[i];
                    soid[i * 2] = hx[(c >> 4) & 0xF];
                    soid[i * 2 + 1] = hx[c & 0xF];
                }
                std::fprintf(stderr, "[CAT] REL row oid=%s root=%llu\n", soid.c_str(),
                             (unsigned long long)(row[1].is_null ? 0ull : row[1].u64));
            }
            if (std::memcmp(row[0].bytes.data(), relation_oid.data(), 16) != 0)
                continue;
            std::uint32_t root = static_cast<std::uint32_t>(row[1].u64);
            return root == 0 ? std::optional<std::uint32_t>{} : std::optional<std::uint32_t>{root};
        }
        return std::nullopt;
    }

    std::optional<std::uint32_t>
    CatalogManager::get_relation_root_page_by_name(const std::optional<UuidBytes>& schema_oid,
                                                   const std::string& relation_name) const
    {
        std::fprintf(stderr, "[CAT] get_relation_root_page_by_name rel='%s' schema=%s db='%s'\n",
                     relation_name.c_str(), schema_oid ? "set" : "<null>", db_path_.c_str());
        auto roid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!roid)
            roid = lookup_object_oid(schema_oid, std::string("TABLE"), relation_name);
        if (!roid && schema_oid) {
            // Fallback: derive relation OID deterministically as in create_relation
            UuidBytes derived{};
            std::string key(reinterpret_cast<const char*>(schema_oid->data()), schema_oid->size());
            key += std::string("::") + relation_name;
            std::hash<std::string> h;
            auto v = h(key);
            std::memcpy(derived.data(), &v, std::min(sizeof(v), derived.size()));
            std::fprintf(stderr, "[CAT] fallback derived rel_oid for '%s'\n",
                         relation_name.c_str());
            roid = derived;
        }
        if (!roid) {
            std::fprintf(stderr, "[CAT] object not found: %s in schema=%s\n", relation_name.c_str(),
                         schema_oid ? "set" : "<null>");
            return std::nullopt;
        }
        {
            static const char* hx = "0123456789abcdef";
            std::string soid;
            soid.resize(32);
            for (size_t i = 0; i < 16; ++i) {
                unsigned char c = (unsigned char)(*roid)[i];
                soid[i * 2] = hx[(c >> 4) & 0xF];
                soid[i * 2 + 1] = hx[c & 0xF];
            }
            std::fprintf(stderr,
                         "[CAT] get_relation_root_page_by_name using rel_oid=%s for rel='%s'\n",
                         soid.c_str(), relation_name.c_str());
        }
        auto root = get_relation_root_page(*roid);
        if (!root) {
            std::fprintf(stderr, "[CAT] no root for relation '%s' (schema_oid=%s)\n",
                         relation_name.c_str(), schema_oid ? "set" : "<null>");
        } else {
            std::fprintf(stderr, "[CAT] root for relation '%s' = %u\n", relation_name.c_str(),
                         *root);
        }
        return root;
    }

    std::vector<std::string>
    CatalogManager::list_column_names_by_name(const std::optional<UuidBytes>& schema_oid,
                                              const std::string& relation_name) const
    {
        std::fprintf(stderr, "[CAT] list_column_names_by_name rel='%s' schema=%s db='%s'\n",
                     relation_name.c_str(), schema_oid ? "set" : "<null>", db_path_.c_str());
        std::vector<std::string> out;
        auto roid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!roid)
            roid = lookup_object_oid(schema_oid, std::string("TABLE"), relation_name);
        if (!roid && schema_oid) {
            // Fallback: derive relation OID deterministically as in create_relation
            UuidBytes derived{};
            std::string key(reinterpret_cast<const char*>(schema_oid->data()), schema_oid->size());
            key += std::string("::") + relation_name;
            std::hash<std::string> h;
            auto v = h(key);
            std::memcpy(derived.data(), &v, std::min(sizeof(v), derived.size()));
            roid = derived;
        }
        if (!roid) {
            std::fprintf(stderr,
                         "[CAT] list_column_names_by_name: object not found: %s schema=%s\n",
                         relation_name.c_str(), schema_oid ? "set" : "<null>");
            return out;
        }
        auto cols = list_columns(*roid);
        std::sort(cols.begin(), cols.end(), [](auto& a, auto& b) { return a.first < b.first; });
        for (auto& [pos, name] : cols)
            out.push_back(name);
        std::fprintf(stderr, "[CAT] list_column_names_by_name: %zu columns for '%s'\n", out.size(),
                     relation_name.c_str());
        return out;
    }

    bool CatalogManager::create_domain(const DomainSpec& spec) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_domain_root_page)
            return false;
        // DOMAIN tuple: oid, base_type, length, precision, scale, charset, collate, not_null,
        // default_expr, check_expr
        TupleLayout domain_layout{};
        domain_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::Int64, 8, true, false},     {AttrType::Int64, 8, true, false},
            {AttrType::Int64, 8, true, false},     {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::Int64, 8, true, false},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
        FileMap fmap_d(layout);
        fmap_d.set_base_path(dir, base);
        HeapRelation dom_rel =
            HeapRelation::open(std::move(fmap_d), ps, *hi.sdb_domain_root_page, domain_layout);
        auto make_bytes = [](const std::string& s, bool nullable = false) {
            Value v{};
            if (nullable && s.empty())
                v.is_null = true;
            else {
                v.is_null = false;
                v.bytes = s;
            }
            return v;
        };
        auto make_int = [](std::int64_t x, bool nullable = false) {
            Value v{};
            if (nullable && x == 0)
                v.is_null = true;
            else {
                v.is_null = false;
                v.u64 = static_cast<std::uint64_t>(x);
            }
            return v;
        };
        // For brevity, store oid as bytes of provided hex or derive from name
        UuidBytes oid{};
        if (!spec.oid_hex.empty() && spec.oid_hex.size() == 32) {
            for (int i = 0; i < 16; ++i) {
                std::string byte_hex = spec.oid_hex.substr(i * 2, 2);
                oid[i] = static_cast<std::uint8_t>(std::strtoul(byte_hex.c_str(), nullptr, 16));
            }
        } else {
            std::hash<std::string> h;
            auto v = h(spec.base_type + std::to_string(spec.length));
            std::memcpy(oid.data(), &v, std::min(sizeof(v), oid.size()));
        }
        Value oidv{};
        oidv.is_null = false;
        oidv.bytes.assign(reinterpret_cast<const char*>(oid.data()), oid.size());
        dom_rel.insert({oidv, make_bytes(spec.base_type), make_int(spec.length, true),
                        make_int(spec.precision, true), make_int(spec.scale, true),
                        make_bytes(spec.charset, true), make_bytes(spec.collate, true),
                        make_int(spec.not_null ? 1 : 0, true), make_bytes(spec.default_expr, true),
                        make_bytes(spec.check_expr, true)});
        return true;
    }

    std::vector<std::tuple<std::string, std::string, std::int64_t, std::int64_t, std::int64_t>>
    CatalogManager::list_domains() const
    {
        std::vector<std::tuple<std::string, std::string, std::int64_t, std::int64_t, std::int64_t>>
            out;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_domain_root_page)
            return out;
        TupleLayout domain_layout{};
        domain_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::Int64, 8, true, false},     {AttrType::Int64, 8, true, false},
            {AttrType::Int64, 8, true, false},     {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::Int64, 8, true, false},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
        FileMap fmap_d(layout);
        fmap_d.set_base_path(dir, base);
        HeapRelation dom_rel =
            HeapRelation::open(std::move(fmap_d), ps, *hi.sdb_domain_root_page, domain_layout);
        auto scan = dom_rel.open_scan();
        std::vector<Value> row;
        while (scan.next(row, nullptr)) {
            if (row.size() < 5)
                continue;
            std::string name; // no name column yet; derive from base_type
            std::string base_type = row[1].is_null ? std::string() : row[1].bytes;
            std::int64_t length = row[2].is_null ? 0 : static_cast<std::int64_t>(row[2].u64);
            std::int64_t precision = row[3].is_null ? 0 : static_cast<std::int64_t>(row[3].u64);
            std::int64_t scale = row[4].is_null ? 0 : static_cast<std::int64_t>(row[4].u64);
            out.emplace_back(name, base_type, length, precision, scale);
        }
        return out;
    }

    bool CatalogManager::set_index_root(const std::optional<UuidBytes>& schema_oid,
                                        const std::string& index_name, std::uint32_t root_page)
    {
        // Store as SDB$STATS JSON under object_oid=index oid
        auto oid = lookup_object_oid(schema_oid, std::string("INDEX"), index_name);
        if (!oid)
            return false;
        std::string json = std::string("{\"root\":") + std::to_string(root_page) + "}";
        return set_stats(*oid, json);
    }

    std::optional<std::uint32_t>
    CatalogManager::get_index_root(const std::optional<UuidBytes>& schema_oid,
                                   const std::string& index_name) const
    {
        std::fprintf(stderr, "[INDEX ROOT] Looking up root for index '%s'\\n", index_name.c_str());
        auto oid = lookup_object_oid(schema_oid, std::string("INDEX"), index_name);
        if (!oid) {
            std::fprintf(stderr, "[INDEX ROOT] Index '%s' not found in catalog\\n",
                         index_name.c_str());
            return std::nullopt;
        }
        std::fprintf(stderr, "[INDEX ROOT] Found index '%s', getting stats\\n", index_name.c_str());
        auto js = get_stats(*oid);
        if (!js || js->empty()) {
            std::fprintf(stderr, "[INDEX ROOT] No stats found for index '%s'\\n",
                         index_name.c_str());
            return std::nullopt;
        }
        std::fprintf(stderr, "[INDEX ROOT] Stats for index '%s': %s\\n", index_name.c_str(),
                     js->c_str());
        // naive parse for "root":N
        auto p = js->find("\"root\"");
        if (p == std::string::npos)
            return std::nullopt;
        auto c = js->find(':', p);
        if (c == std::string::npos)
            return std::nullopt;
        std::string tail = js->substr(c + 1);
        std::uint32_t v = 0;
        try {
            v = static_cast<std::uint32_t>(std::stoul(tail));
        } catch (...) {
            return std::nullopt;
        }
        return v;
    }

    bool CatalogManager::create_view(const UuidBytes& schema_oid, const std::string& name,
                                     const std::string& definition_sql) const
    {
        // Insert SDB$OBJECT(type=VIEW) and SDB$SOURCE(object_oid,text)
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page)
            return false;
        // Derive OID from schema+name for now
        UuidBytes oid{};
        {
            std::hash<std::string> h;
            auto v =
                h(std::string(reinterpret_cast<const char*>(schema_oid.data()), schema_oid.size()) +
                  std::string("::VIEW::") + name);
            std::memcpy(oid.data(), &v, std::min(sizeof(v), oid.size()));
        }
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        FileMap fmap_o(layout);
        fmap_o.set_base_path(dir, base);
        auto object_rel =
            HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, object_layout);
        Value oidv{};
        oidv.is_null = false;
        oidv.bytes.assign(reinterpret_cast<const char*>(oid.data()), oid.size());
        Value typev{};
        typev.is_null = false;
        typev.bytes = "VIEW";
        Value schemav{};
        schemav.is_null = false;
        schemav.bytes.assign(reinterpret_cast<const char*>(schema_oid.data()), schema_oid.size());
        Value namev{};
        namev.is_null = false;
        namev.bytes = name;
        object_rel.insert({oidv, typev, schemav, namev});
        // SDB$SOURCE(object_oid,text,doc)
        if (hi.sdb_source_root_page) {
            TupleLayout source_layout{};
            source_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, false},
                                   {AttrType::VarBytes, 0, false, true}};
            FileMap fmap_s(layout);
            fmap_s.set_base_path(dir, base);
            auto src_rel =
                HeapRelation::open(std::move(fmap_s), ps, *hi.sdb_source_root_page, source_layout);
            Value obj{};
            obj.is_null = false;
            obj.bytes.assign(reinterpret_cast<const char*>(oid.data()), oid.size());
            Value txt{};
            txt.is_null = false;
            txt.bytes = definition_sql;
            Value doc{};
            doc.is_null = true;
            src_rel.insert({obj, txt, doc});
        }
        return true;
    }

    std::vector<std::pair<UuidBytes, std::string>>
    CatalogManager::list_views(const std::optional<UuidBytes>& schema_oid) const
    {
        std::vector<std::pair<UuidBytes, std::string>> out;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page)
            return out;
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        FileMap fmap_o(layout);
        fmap_o.set_base_path(dir, base);
        auto object_rel =
            HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, object_layout);
        auto scan = object_rel.open_scan();
        std::vector<Value> row;
        while (scan.next(row, nullptr)) {
            if (row.size() < 4)
                continue;
            if (row[1].is_null || row[1].bytes != std::string("VIEW"))
                continue;
            if (schema_oid.has_value()) {
                if (row[2].is_null || row[2].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[2].bytes.data(), schema_oid->data(), 16) != 0)
                    continue;
            }
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            UuidBytes oid{};
            std::memcpy(oid.data(), row[0].bytes.data(), 16);
            out.emplace_back(oid, row[3].bytes);
        }
        return out;
    }

    std::string CatalogManager::get_view_definition(const std::optional<UuidBytes>& schema_oid,
                                                    const std::string& name) const
    {
        // Find view OID in SDB$OBJECT
        auto oid = lookup_object_oid(schema_oid, std::string("VIEW"), name);
        if (!oid)
            return std::string();
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_source_root_page)
            return std::string();
        TupleLayout source_layout{};
        source_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true}};
        FileMap fmap_s(layout);
        fmap_s.set_base_path(dir, base);
        auto src_rel =
            HeapRelation::open(std::move(fmap_s), ps, *hi.sdb_source_root_page, source_layout);
        auto scan = src_rel.open_scan();
        std::vector<Value> row;
        while (scan.next(row, nullptr)) {
            if (row.size() < 2)
                continue;
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            if (std::memcmp(row[0].bytes.data(), oid->data(), 16) != 0)
                continue;
            return row[1].is_null ? std::string() : row[1].bytes;
        }
        return std::string();
    }

    std::vector<CatalogManager::IndexCatalogInfo>
    CatalogManager::list_relation_indexes_by_name(const std::optional<UuidBytes>& schema_oid,
                                                  const std::string& relation_name) const
    {
        std::vector<IndexCatalogInfo> out;
        // Resolve relation OID
        auto rel_oid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!rel_oid)
            rel_oid = lookup_object_oid(schema_oid, std::string("TABLE"), relation_name);
        if (!rel_oid)
            return out;
        // Open header and layout
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        FileMap fmap(layout);
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page || !hi.sdb_relation_root_page)
            return out;
        // Read SDB$OBJECT for INDEX rows belonging to relation_oid
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        FileMap fmap_o(layout);
        fmap_o.set_base_path(dir, base);
        auto object_rel =
            HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, object_layout);
        // Index metadata tables
        TupleLayout index_layout{};
        index_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true}};
        FileMap fmap_i(layout);
        fmap_i.set_base_path(dir, base);
        auto idx_rel =
            HeapRelation::open(std::move(fmap_i), ps, *hi.sdb_relation_root_page, index_layout);
        TupleLayout key_layout{};
        key_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::Int64, 8, true, false},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
        FileMap fmap_k(layout);
        fmap_k.set_base_path(dir, base);
        auto key_rel =
            HeapRelation::open(std::move(fmap_k), ps, *hi.sdb_relation_root_page, key_layout);
        // Build map of index_oid -> (name, method, unique)
        std::unordered_map<std::string, IndexCatalogInfo> idx_map;
        {
            auto scan = object_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (scan.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("INDEX"))
                    continue;
                // Skip schema check for indexes in SDB$OBJECT - we'll filter by relation_oid in
                // SDB$INDEX instead
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                std::string oid(row[0].bytes.data(), row[0].bytes.size());
                IndexCatalogInfo info{};
                info.name = row[3].is_null ? std::string() : row[3].bytes;
                idx_map.emplace(std::move(oid), std::move(info));
            }
        }
        if (idx_map.empty())
            return out;
        // Join with SDB$INDEX to fetch method/unique
        {
            auto scan = idx_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (scan.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                // Filter by relation_oid (row[1] should be the relation this index belongs to)
                if (row[1].is_null || row[1].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[1].bytes.data(), rel_oid->data(), 16) != 0)
                    continue;
                std::string oid(row[0].bytes.data(), row[0].bytes.size());
                auto it = idx_map.find(oid);
                if (it == idx_map.end())
                    continue;
                it->second.method = row[3].is_null ? std::string() : row[3].bytes;
                std::string uniq = row[2].is_null ? std::string() : row[2].bytes;
                it->second.unique = (uniq == "TRUE");
            }
        }
        // Join with SDB$INDEX_KEY to collect key columns (ordered by position) and expressions
        {
            auto scan = key_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (scan.next(row, &rid)) {
                if (row.size() < 6)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                std::string oid(row[0].bytes.data(), row[0].bytes.size());
                auto it = idx_map.find(oid);
                if (it == idx_map.end())
                    continue;
                std::int64_t pos = row[1].is_null ? 0 : static_cast<std::int64_t>(row[1].u64);
                std::string col = row[2].is_null ? std::string() : row[2].bytes;
                std::string dir = row[4].is_null ? std::string() : row[4].bytes; // ASC/DESC
                // Skip invalid positions
                if (pos <= 0)
                    continue;
                // Ensure keys vector is large enough (pos is 1-based)
                if ((std::size_t)pos > it->second.keys.size())
                    it->second.keys.resize((std::size_t)pos);
                it->second.keys[(std::size_t)pos - 1] = {col, dir};
                // row[5] may contain expression text when functional index
                if (!row[5].is_null && !row[5].bytes.empty()) {
                    it->second.exprs.push_back(row[5].bytes);
                }
            }
        }
        // Emit
        for (auto& kv : idx_map)
            out.push_back(std::move(kv.second));
        return out;
    }

    std::vector<CatalogManager::ConstraintInfo>
    CatalogManager::list_relation_constraints_by_name(const std::optional<UuidBytes>& schema_oid,
                                                      const std::string& relation_name) const
    {
        std::vector<ConstraintInfo> out;
        auto rel_oid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!rel_oid)
            rel_oid = lookup_object_oid(schema_oid, std::string("TABLE"), relation_name);
        if (!rel_oid)
            return out;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_relation_root_page)
            return out;
        // Define layouts for SDB$OBJECT, SDB$CONSTRAINT, and SDB$CONSTRAINT_KEY
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        TupleLayout c_layout{};
        c_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
        TupleLayout ck_layout{};
        ck_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, // constraint_oid
            {AttrType::Int64, 8, true, false},     // position
            {AttrType::VarBytes, 0, false, true},  // column_oid (or name)
            {AttrType::VarBytes, 0, false, true}   // ref_column_oid (or name)
        };
        FileMap fmap_o(layout);
        fmap_o.set_base_path(dir, base);
        FileMap fmap_c(layout);
        fmap_c.set_base_path(dir, base);
        FileMap fmap_ck(layout);
        fmap_ck.set_base_path(dir, base);
        auto obj_rel =
            HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, object_layout);
        auto cons_rel =
            HeapRelation::open(std::move(fmap_c), ps, *hi.sdb_relation_root_page, c_layout);
        auto key_rel =
            HeapRelation::open(std::move(fmap_ck), ps, *hi.sdb_relation_root_page, ck_layout);
        std::unordered_map<std::string, ConstraintInfo> cmap;
        std::unordered_map<std::string, std::string> name_map;
        {
            auto scan = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (scan.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("CONSTRAINT"))
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                std::string oid(row[0].bytes.data(), row[0].bytes.size());
                name_map.emplace(oid, row[3].is_null ? std::string() : row[3].bytes);
            }
        }
        {
            auto scan = cons_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (scan.next(row, &rid)) {
                if (row.size() < 10)
                    continue;
                if (row[1].is_null || row[1].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[1].bytes.data(), rel_oid->data(), 16) != 0)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                std::string coid(row[0].bytes.data(), row[0].bytes.size());
                ConstraintInfo ci{};
                auto itn = name_map.find(coid);
                if (itn != name_map.end())
                    ci.name = itn->second;
                ci.type = row[2].is_null ? std::string() : row[2].bytes;
                ci.deferrable = (!row[3].is_null && row[3].bytes == "TRUE");
                ci.initially_deferred = (!row[4].is_null && row[4].bytes == "TRUE");
                ci.check_expr = row[5].is_null ? std::string() : row[5].bytes;
                if (!row[7].is_null && row[7].bytes.size() == 16) {
                    std::string rroid(row[7].bytes.data(), row[7].bytes.size());
                    auto itnm = name_map.find(rroid);
                    if (itnm != name_map.end())
                        ci.ref_relation = itnm->second;
                }
                ci.on_delete = row[8].is_null ? std::string() : row[8].bytes;
                ci.on_update = row[9].is_null ? std::string() : row[9].bytes;
                cmap.emplace(std::move(coid), std::move(ci));
            }
        }
        if (cmap.empty())
            return out;
        {
            auto scan = key_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (scan.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                std::string coid(row[0].bytes.data(), row[0].bytes.size());
                auto it = cmap.find(coid);
                if (it == cmap.end())
                    continue;
                std::string col = row[2].is_null ? std::string() : row[2].bytes;
                std::string rcol = row[3].is_null ? std::string() : row[3].bytes;
                if (!col.empty())
                    it->second.columns.push_back(col);
                if (!rcol.empty())
                    it->second.ref_columns.push_back(rcol);
            }
        }
        for (auto& kv : cmap)
            out.push_back(std::move(kv.second));
        return out;
    }

    bool CatalogManager::create_constraint_catalog(
        const std::optional<UuidBytes>& schema_oid, const UuidBytes& relation_oid,
        const std::string& name, const std::string& type, bool deferrable, bool initially_deferred,
        const std::string& check_expr, const std::vector<std::string>& columns,
        const std::vector<std::string>& ref_columns, const std::string& ref_relation,
        const std::string& on_update, const std::string& on_delete) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page || !hi.sdb_relation_root_page)
            return false;
        // Generate constraint OID
        UuidBytes coid{};
        {
            std::hash<std::string> h;
            auto v = h(name + std::string(reinterpret_cast<const char*>(relation_oid.data()),
                                          relation_oid.size()));
            std::memcpy(coid.data(), &v, std::min(sizeof(v), coid.size()));
        }
        // OBJECT row: (oid, type=CONSTRAINT, schema_oid, name)
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, false}};
        FileMap fmap_o(layout);
        fmap_o.set_base_path(dir, base);
        auto obj_rel =
            HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, obj_layout);
        auto make_uuid = [](const UuidBytes& u) {
            Value v{};
            v.is_null = false;
            v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
            return v;
        };
        auto make_opt_uuid = [](const std::optional<UuidBytes>& u) {
            Value v{};
            if (u) {
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u->data()), u->size());
            } else
                v.is_null = true;
            return v;
        };
        auto make_str = [](const std::string& s) {
            Value v{};
            v.is_null = false;
            v.bytes = s;
            return v;
        };
        obj_rel.insert(
            {make_uuid(coid), make_str("CONSTRAINT"), make_opt_uuid(schema_oid), make_str(name)});
        // CONSTRAINT row in SDB$CONSTRAINT (under relation root)
        TupleLayout c_layout{};
        c_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
        FileMap fmap_c(layout);
        fmap_c.set_base_path(dir, base);
        auto cons_rel =
            HeapRelation::open(std::move(fmap_c), ps, *hi.sdb_relation_root_page, c_layout);
        // Resolve referenced relation OID if provided
        Value ref_rel_val{};
        ref_rel_val.is_null = true;
        if (!ref_relation.empty()) {
            std::string ref_schema = std::string();
            std::string ref_table = ref_relation;
            auto dotp = ref_relation.find('.');
            if (dotp != std::string::npos) {
                ref_schema = ref_relation.substr(0, dotp);
                ref_table = ref_relation.substr(dotp + 1);
            }
            std::optional<UuidBytes> soid_ref;
            if (!ref_schema.empty())
                soid_ref = lookup_schema_oid_by_name(ref_schema);
            if (!soid_ref)
                soid_ref = schema_oid; // fallback to same schema
            if (soid_ref) {
                auto roid_ref = lookup_object_oid(soid_ref, std::string("RELATION"), ref_table);
                if (!roid_ref)
                    roid_ref = lookup_object_oid(soid_ref, std::string("TABLE"), ref_table);
                if (roid_ref) {
                    ref_rel_val.is_null = false;
                    ref_rel_val.bytes.assign(reinterpret_cast<const char*>(roid_ref->data()),
                                             roid_ref->size());
                }
            }
        }
        // Ensure backing unique indexes for PK/UNIQUE and validate FK target uniqueness
        // (best-effort)
        if (type == std::string("PRIMARY_KEY") || type == std::string("UNIQUE")) {
            std::string iname = std::string(type == "PRIMARY_KEY" ? "pk_" : "ux_") + name;
            std::vector<std::pair<std::string, std::string>> keys;
            keys.reserve(columns.size());
            for (const auto& c : columns)
                keys.emplace_back(c, std::string("ASC"));
            create_index_catalog(schema_oid, relation_oid, iname, "BTREE", keys, /*unique*/ true);
        }
        if (type == std::string("FOREIGN_KEY") && !ref_relation.empty()) {
            // Optional check: referenced relation has some UNIQUE/PK; do not fail, just best-effort
            // guard
            std::string ref_schema, ref_table = ref_relation;
            auto dotp = ref_relation.find('.');
            if (dotp != std::string::npos) {
                ref_schema = ref_relation.substr(0, dotp);
                ref_table = ref_relation.substr(dotp + 1);
            }
            auto soid_ref = ref_schema.empty() ? schema_oid : lookup_schema_oid_by_name(ref_schema);
            if (soid_ref) {
                auto cons = list_relation_constraints_by_name(soid_ref, ref_table);
                bool has_unique = false;
                for (const auto& c : cons) {
                    if (c.type == "PRIMARY_KEY" || c.type == "UNIQUE") {
                        has_unique = true;
                        break;
                    }
                }
                (void)has_unique; // future: error/report if false
            }
        }
        cons_rel.insert({make_uuid(coid), make_uuid(relation_oid), make_str(type),
                         make_str(deferrable ? "TRUE" : "FALSE"),
                         make_str(initially_deferred ? "TRUE" : "FALSE"), make_str(check_expr),
                         Value{}, ref_rel_val, make_str(on_delete), make_str(on_update)});
        // Keys in SDB$CONSTRAINT_KEY
        TupleLayout ck_layout{};
        ck_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                           {AttrType::Int64, 8, true, false},
                           {AttrType::VarBytes, 0, false, true},
                           {AttrType::VarBytes, 0, false, true}};
        FileMap fmap_ck(layout);
        fmap_ck.set_base_path(dir, base);
        auto key_rel =
            HeapRelation::open(std::move(fmap_ck), ps, *hi.sdb_relation_root_page, ck_layout);
        for (size_t i = 0; i < columns.size(); ++i) {
            Value vpos{};
            vpos.is_null = false;
            vpos.u64 = (std::uint64_t)(i + 1);
            Value vcol{};
            vcol.is_null = columns[i].empty();
            vcol.bytes = columns[i];
            Value vrcol{};
            vrcol.is_null = (i >= ref_columns.size() || ref_columns[i].empty());
            if (!vrcol.is_null)
                vrcol.bytes = ref_columns[i];
            key_rel.insert({make_uuid(coid), vpos, vcol, vrcol});
        }
        return true;
    }

    bool CatalogManager::drop_constraint_by_name(const std::optional<UuidBytes>& schema_oid,
                                                 const std::string& relation_name,
                                                 const std::string& constraint_name) const
    {
        // Resolve relation and constraint OIDs
        auto rel_oid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!rel_oid)
            rel_oid = lookup_object_oid(schema_oid, std::string("TABLE"), relation_name);
        if (!rel_oid)
            return false;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page || !hi.sdb_relation_root_page)
            return false;
        // Layouts
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, false}};
        TupleLayout c_layout{};
        c_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
        TupleLayout ck_layout{};
        ck_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                           {AttrType::Int64, 8, true, false},
                           {AttrType::VarBytes, 0, false, true},
                           {AttrType::VarBytes, 0, false, true}};
        FileMap fo1(layout);
        fo1.set_base_path(dir, base);
        FileMap fc1(layout);
        fc1.set_base_path(dir, base);
        FileMap fk1(layout);
        fk1.set_base_path(dir, base);
        auto obj_rel = HeapRelation::open(std::move(fo1), ps, *hi.sdb_object_root_page, obj_layout);
        auto cons_rel =
            HeapRelation::open(std::move(fc1), ps, *hi.sdb_relation_root_page, c_layout);
        auto key_rel =
            HeapRelation::open(std::move(fk1), ps, *hi.sdb_relation_root_page, ck_layout);
        // Find constraint OID by name and relation
        std::optional<UuidBytes> coid;
        {
            auto scan = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (scan.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("CONSTRAINT"))
                    continue;
                if (row[3].is_null || row[3].bytes != constraint_name)
                    continue;
                // For safety we will verify relation match in constraint table
                UuidBytes tmp{};
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                std::memcpy(tmp.data(), row[0].bytes.data(), 16);
                // verify relation match
                auto sc2 = cons_rel.open_scan();
                std::vector<Value> r2;
                ods::RowId rr{};
                bool match = false;
                while (sc2.next(r2, &rr)) {
                    if (r2.size() < 2)
                        continue;
                    if (r2[0].is_null || r2[0].bytes.size() != 16)
                        continue;
                    if (std::memcmp(r2[0].bytes.data(), tmp.data(), 16) != 0)
                        continue;
                    if (r2[1].is_null || r2[1].bytes.size() != 16)
                        continue;
                    if (std::memcmp(r2[1].bytes.data(), rel_oid->data(), 16) == 0) {
                        match = true;
                        break;
                    }
                }
                if (match) {
                    coid = tmp;
                    break;
                }
            }
        }
        if (!coid)
            return false;
        // Collect rowids to delete from CONSTRAINT and CONSTRAINT_KEY
        std::vector<ods::RowId> cons_rows;
        std::vector<ods::RowId> key_rows;
        {
            auto sc = cons_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 1)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), coid->data(), 16) == 0)
                    cons_rows.push_back(rid);
            }
        }
        {
            auto sc = key_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 1)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), coid->data(), 16) == 0)
                    key_rows.push_back(rid);
            }
        }
        // Find OBJECT rowid to delete
        std::optional<ods::RowId> obj_rowid;
        {
            auto sc = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("CONSTRAINT"))
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), coid->data(), 16) == 0) {
                    obj_rowid = rid;
                    break;
                }
            }
        }
        // Delete in order: keys -> constraints -> object
        bool ok = true;
        for (auto& kr : key_rows)
            ok = ok && key_rel.remove(kr);
        for (auto& cr : cons_rows)
            ok = ok && cons_rel.remove(cr);
        if (obj_rowid)
            ok = ok && obj_rel.remove(*obj_rowid);
        return ok;
    }

    std::vector<CatalogManager::TriggerInfo>
    CatalogManager::list_relation_triggers_by_name(const std::optional<UuidBytes>& schema_oid,
                                                   const std::string& relation_name) const
    {
        std::fprintf(stderr, "[CAT] list_relation_triggers_by_name rel='%s' schema=%s db='%s'\n",
                     relation_name.c_str(), schema_oid ? "set" : "<null>", db_path_.c_str());
        std::vector<TriggerInfo> out;
        auto rel_oid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!rel_oid) {
            std::fprintf(stderr, "[CAT] triggers: object not found: %s schema=%s\n",
                         relation_name.c_str(), schema_oid ? "set" : "<null>");
            return out;
        }
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_relation_root_page)
            return out;
        // Open object table to resolve trigger names
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},  // oid
                            {AttrType::VarBytes, 0, false, false},  // type
                            {AttrType::VarBytes, 0, false, true},   // schema_oid
                            {AttrType::VarBytes, 0, false, false}}; // name
        FileMap fmap_o(layout);
        fmap_o.set_base_path(dir, base);
        auto obj_rel =
            HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, obj_layout);
        TupleLayout t_layout{};
        t_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, // oid
            {AttrType::VarBytes, 0, false, false}, // relation_oid
            {AttrType::VarBytes, 0, false, false}, // timing
            {AttrType::VarBytes, 0, false, false}, // events
            {AttrType::Int64, 8, true, false},     // position
            {AttrType::VarBytes, 0, false, true},  // for_each
            {AttrType::VarBytes, 0, false, true},  // active
            {AttrType::VarBytes, 0, false, true}   // update_of
        };
        FileMap fmap_t(layout);
        fmap_t.set_base_path(dir, base);
        auto t_rel =
            HeapRelation::open(std::move(fmap_t), ps, *hi.sdb_relation_root_page, t_layout);
        // Build a small lookup for oid->name for triggers encountered
        auto resolve_name = [&](const UuidBytes& oid_bytes) -> std::string {
            auto scn = obj_rel.open_scan();
            std::vector<Value> orow;
            ods::RowId rr{};
            while (scn.next(orow, &rr)) {
                if (orow.size() < 4)
                    continue;
                if (orow[0].is_null || orow[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(orow[0].bytes.data(), oid_bytes.data(), 16) != 0)
                    continue;
                if (orow[1].is_null || orow[1].bytes != std::string("TRIGGER"))
                    continue;
                return orow[3].is_null ? std::string() : orow[3].bytes;
            }
            return std::string();
        };
        auto scan = t_rel.open_scan();
        std::vector<Value> row;
        ods::RowId rid{};
        while (scan.next(row, &rid)) {
            if (row.size() < 5)
                continue;
            if (row[1].is_null || row[1].bytes.size() != 16)
                continue;
            if (std::memcmp(row[1].bytes.data(), rel_oid->data(), 16) != 0)
                continue;
            TriggerInfo ti{};
            if (!row[0].is_null && row[0].bytes.size() == 16) {
                UuidBytes toid{};
                std::memcpy(toid.data(), row[0].bytes.data(), 16);
                ti.oid = toid;
                ti.name = resolve_name(toid);
            }
            ti.timing = row[2].is_null ? std::string() : row[2].bytes;
            ti.events = row[3].is_null ? std::string() : row[3].bytes;
            ti.position = row[4].is_null ? 0 : (int)row[4].u64;
            // optional new fields
            if (row.size() > 5)
                ti.for_each = row[5].is_null ? std::string() : row[5].bytes;
            if (row.size() > 6) {
                ti.active =
                    row[6].is_null ? true : (row[6].bytes == "TRUE" || row[6].bytes == "true");
            }
            if (row.size() > 7 && !row[7].is_null) {
                // split update_of JSON list naively by commas (stored as simple CSV-like JSON)
                std::string s = row[7].bytes;
                size_t p = 0;
                while (p < s.size()) {
                    auto c = s.find(',', p);
                    auto tok = s.substr(p, c == std::string::npos ? std::string::npos : c - p);
                    if (!tok.empty())
                        ti.update_of_cols.push_back(tok);
                    if (c == std::string::npos)
                        break;
                    p = c + 1;
                }
            }
            out.push_back(std::move(ti));
        }
        std::fprintf(stderr, "[CAT] triggers listed: %zu for '%s'\n", out.size(),
                     relation_name.c_str());
        return out;
    }

    std::string CatalogManager::get_source_for_object(const UuidBytes& object_oid) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_source_root_page)
            return {};
        TupleLayout src_layout{};
        src_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true}};
        FileMap fs(layout);
        fs.set_base_path(dir, base);
        auto src_rel = HeapRelation::open(std::move(fs), ps, *hi.sdb_source_root_page, src_layout);
        auto sc = src_rel.open_scan();
        std::vector<Value> row;
        ods::RowId rid{};
        while (sc.next(row, &rid)) {
            if (row.size() < 2)
                continue;
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            if (std::memcmp(row[0].bytes.data(), object_oid.data(), 16) != 0)
                continue;
            return row[1].is_null ? std::string() : row[1].bytes;
        }
        return {};
    }

    bool CatalogManager::create_trigger_catalog(const std::optional<UuidBytes>& schema_oid,
                                                const UuidBytes& relation_oid,
                                                const std::string& name, const std::string& timing,
                                                const std::string& events_json, int position,
                                                const std::string& for_each, bool active,
                                                const std::string& update_of_json,
                                                const std::string& body_source) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page || !hi.sdb_relation_root_page || !hi.sdb_source_root_page)
            return false;
        // OBJECT: (oid, type=TRIGGER, schema_oid, name)
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, false}};
        FileMap fo1(layout);
        fo1.set_base_path(dir, base);
        auto obj_rel = HeapRelation::open(std::move(fo1), ps, *hi.sdb_object_root_page, obj_layout);
        auto make_uuid = [&](const UuidBytes& u) {
            Value v{};
            v.is_null = false;
            v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
            return v;
        };
        auto make_opt_uuid = [&](const std::optional<UuidBytes>& u) {
            Value v{};
            if (u) {
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u->data()), u->size());
            } else
                v.is_null = true;
            return v;
        };
        auto make_str = [&](const std::string& s) {
            Value v{};
            v.is_null = false;
            v.bytes = s;
            return v;
        };
        UuidBytes toid{};
        {
            std::hash<std::string> h;
            auto v = h(name + ":TRIGGER");
            std::memcpy(toid.data(), &v, std::min(sizeof(v), toid.size()));
        }
        obj_rel.insert(
            {make_uuid(toid), make_str("TRIGGER"), make_opt_uuid(schema_oid), make_str(name)});
        // TRIGGER table (oid, relation_oid, timing, events, position, for_each, active, update_of)
        TupleLayout trg_layout{};
        trg_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::Int64, 8, true, false},     {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true}};
        FileMap ft1(layout);
        ft1.set_base_path(dir, base);
        auto trg_rel =
            HeapRelation::open(std::move(ft1), ps, *hi.sdb_relation_root_page, trg_layout);
        Value vpos{};
        vpos.is_null = false;
        vpos.u64 = (std::uint64_t)position;
        Value vact{};
        vact.is_null = false;
        vact.bytes = active ? "TRUE" : "FALSE";
        trg_rel.insert({make_uuid(toid), make_uuid(relation_oid), make_str(timing),
                        make_str(events_json), vpos, make_str(for_each), vact,
                        make_str(update_of_json)});
        // SOURCE table for body
        TupleLayout src_layout{};
        src_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true}};
        FileMap fs1(layout);
        fs1.set_base_path(dir, base);
        auto src_rel = HeapRelation::open(std::move(fs1), ps, *hi.sdb_source_root_page, src_layout);
        src_rel.insert({make_uuid(toid), make_str(body_source), make_str(std::string())});
        return true;
    }

    bool CatalogManager::drop_trigger_by_name(const std::optional<UuidBytes>& schema_oid,
                                              const std::string& relation_name,
                                              const std::string& trigger_name) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page || !hi.sdb_relation_root_page || !hi.sdb_source_root_page)
            return false;
        // Layouts
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, false}};
        TupleLayout trg_layout{};
        trg_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::Int64, 8, true, false}};
        TupleLayout src_layout{};
        src_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true}};
        // Open relations
        FileMap fo1(layout);
        fo1.set_base_path(dir, base);
        auto obj_rel = HeapRelation::open(std::move(fo1), ps, *hi.sdb_object_root_page, obj_layout);
        FileMap ft1(layout);
        ft1.set_base_path(dir, base);
        auto trg_rel =
            HeapRelation::open(std::move(ft1), ps, *hi.sdb_relation_root_page, trg_layout);
        FileMap fs1(layout);
        fs1.set_base_path(dir, base);
        auto src_rel = HeapRelation::open(std::move(fs1), ps, *hi.sdb_source_root_page, src_layout);
        // Resolve trigger object oid by (schema_oid, type=TRIGGER, name)
        std::optional<UuidBytes> trig_oid_opt;
        {
            auto sc = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("TRIGGER"))
                    continue;
                if (!schema_oid && !row[2].is_null)
                    continue;
                if (schema_oid && (row[2].is_null || row[2].bytes.size() != 16 ||
                                   std::memcmp(row[2].bytes.data(), schema_oid->data(), 16) != 0))
                    continue;
                if (row[3].is_null || row[3].bytes != trigger_name)
                    continue;
                UuidBytes o{};
                if (!row[0].is_null && row[0].bytes.size() == 16)
                    std::memcpy(o.data(), row[0].bytes.data(), 16);
                trig_oid_opt = o;
                break;
            }
        }
        if (!trig_oid_opt)
            return false;
        // Delete TRIGGER rows matching oid
        {
            auto sc = trg_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 5)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), trig_oid_opt->data(), 16) == 0) {
                    trg_rel.remove(rid);
                }
            }
        }
        // Delete SOURCE rows matching oid
        {
            auto sc = src_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 3)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), trig_oid_opt->data(), 16) == 0) {
                    src_rel.remove(rid);
                }
            }
        }
        // Delete OBJECT row
        {
            auto sc = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("TRIGGER"))
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), trig_oid_opt->data(), 16) == 0) {
                    obj_rel.remove(rid);
                    break;
                }
            }
        }
        return true;
    }

    bool CatalogManager::alter_trigger_active(const std::optional<UuidBytes>& schema_oid,
                                              const std::string& trigger_name, bool active) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page || !hi.sdb_relation_root_page)
            return false;
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, false}};
        TupleLayout trg_layout{};
        trg_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::Int64, 8, true, false},     {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true}};
        FileMap fo1(layout);
        fo1.set_base_path(dir, base);
        auto obj_rel = HeapRelation::open(std::move(fo1), ps, *hi.sdb_object_root_page, obj_layout);
        FileMap ft1(layout);
        ft1.set_base_path(dir, base);
        auto trg_rel =
            HeapRelation::open(std::move(ft1), ps, *hi.sdb_relation_root_page, trg_layout);
        // Resolve trigger oid by name (and schema if provided)
        std::optional<UuidBytes> toid;
        {
            auto sc = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("TRIGGER"))
                    continue;
                if (schema_oid) {
                    if (row[2].is_null || row[2].bytes.size() != 16 ||
                        std::memcmp(row[2].bytes.data(), schema_oid->data(), 16) != 0)
                        continue;
                }
                if (row[3].is_null || row[3].bytes != trigger_name)
                    continue;
                UuidBytes o{};
                if (!row[0].is_null && row[0].bytes.size() == 16)
                    std::memcpy(o.data(), row[0].bytes.data(), 16);
                toid = o;
                break;
            }
        }
        if (!toid)
            return false;
        // Scan TRIGGER table to update ACTIVE flag (7th attr index 6)
        auto sc = trg_rel.open_scan();
        std::vector<Value> row;
        ods::RowId rid{};
        while (sc.next(row, &rid)) {
            if (row.size() < 8)
                continue;
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            if (std::memcmp(row[0].bytes.data(), toid->data(), 16) != 0)
                continue;
            Value vact{};
            vact.is_null = false;
            vact.bytes = active ? "TRUE" : "FALSE";
            row[6] = vact;
            ods::RowId newrid{};
            trg_rel.update(rid, row, &newrid);

            // Force flush to ensure immediate visibility
            {
                FileOptions fo_flush{};
                fo_flush.direct_io = false;
                auto fh_flush = FileManager::open(db_path_ + ".seg0", fo_flush, false);
                FileManager::flush(fh_flush);
            }
            return true;
        }
        return false;
    }

    std::vector<CatalogManager::ForeignKeyInfo>
    CatalogManager::list_inbound_foreign_keys_by_parent(const UuidBytes& parent_relation_oid) const
    {
        std::vector<ForeignKeyInfo> out;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_relation_root_page || !hi.sdb_object_root_page)
            return out;
        // Layouts
        TupleLayout cons_layout{};
        cons_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, // oid
            {AttrType::VarBytes, 0, false, false}, // relation_oid
            {AttrType::VarBytes, 0, false, false}, // type
            {AttrType::VarBytes, 0, false, false}, // deferrable
            {AttrType::VarBytes, 0, false, false}, // initially_deferred
            {AttrType::VarBytes, 0, false, true},  // check_expr
            {AttrType::VarBytes, 0, false, true},  // index_oid
            {AttrType::VarBytes, 0, false, true},  // ref_relation_oid
            {AttrType::VarBytes, 0, false, true},  // on_delete
            {AttrType::VarBytes, 0, false, true}   // on_update
        };
        TupleLayout key_layout{};
        key_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::Int64, 8, true, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, true}};
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, false}};
        FileMap fc(layout);
        fc.set_base_path(dir, base);
        auto cons_rel =
            HeapRelation::open(std::move(fc), ps, *hi.sdb_relation_root_page, cons_layout);
        FileMap fk(layout);
        fk.set_base_path(dir, base);
        auto key_rel =
            HeapRelation::open(std::move(fk), ps, *hi.sdb_relation_root_page, key_layout);
        FileMap fo2(layout);
        fo2.set_base_path(dir, base);
        auto obj_rel = HeapRelation::open(std::move(fo2), ps, *hi.sdb_object_root_page, obj_layout);
        // Scan constraints referencing parent_relation_oid, type=FOREIGN_KEY
        std::unordered_map<std::string, ForeignKeyInfo> fkmap; // coid->info
        {
            auto sc = cons_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 10)
                    continue;
                if (row[2].is_null || row[2].bytes != std::string("FOREIGN_KEY"))
                    continue;
                if (row[7].is_null || row[7].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[7].bytes.data(), parent_relation_oid.data(), 16) != 0)
                    continue;
                // child relation_oid in row[1]
                ForeignKeyInfo fi{};
                if (!row[1].is_null && row[1].bytes.size() == 16)
                    std::memcpy(fi.child_relation_oid.data(), row[1].bytes.data(), 16);
                fi.on_delete = row[8].is_null ? std::string() : row[8].bytes;
                fi.on_update = row[9].is_null ? std::string() : row[9].bytes;
                fi.deferrable =
                    (!row[3].is_null && (row[3].bytes == "TRUE" || row[3].bytes == "true"));
                fi.initially_deferred =
                    (!row[4].is_null && (row[4].bytes == "TRUE" || row[4].bytes == "true"));
                // resolve constraint name from SDB$OBJECT (type=CONSTRAINT)
                std::string coid(row[0].bytes.data(), row[0].bytes.size());
                fkmap.emplace(coid, std::move(fi));
            }
        }
        if (fkmap.empty())
            return out;
        // Join keys
        {
            auto sc = key_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                std::string coid(row[0].bytes.data(), row[0].bytes.size());
                auto it = fkmap.find(coid);
                if (it == fkmap.end())
                    continue;
                std::string col = row[2].is_null ? std::string() : row[2].bytes;
                std::string rcol = row[3].is_null ? std::string() : row[3].bytes;
                if (!col.empty())
                    it->second.child_columns.push_back(col);
                if (!rcol.empty())
                    it->second.parent_columns.push_back(rcol);
            }
        }
        // Resolve child relation names and constraint names via SDB$OBJECT
        for (auto& kv : fkmap) {
            // find relation name
            auto sc = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("RELATION"))
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), kv.second.child_relation_oid.data(), 16) != 0)
                    continue;
                kv.second.child_relation_name = row[3].is_null ? std::string() : row[3].bytes;
                break;
            }
            // find constraint name
            auto sc2 = obj_rel.open_scan();
            std::vector<Value> row2;
            ods::RowId rid2{};
            while (sc2.next(row2, &rid2)) {
                if (row2.size() < 4)
                    continue;
                if (row2[1].is_null || row2[1].bytes != std::string("CONSTRAINT"))
                    continue;
                if (row2[0].is_null || row2[0].bytes.size() != 16)
                    continue;
                std::string coid = kv.first;
                if (std::memcmp(row2[0].bytes.data(), coid.data(), 16) != 0)
                    continue;
                kv.second.constraint_name = row2[3].is_null ? std::string() : row2[3].bytes;
                break;
            }
            out.push_back(std::move(kv.second));
        }
        return out;
    }

    std::vector<CatalogManager::ForeignKeyInfo>
    CatalogManager::list_inbound_foreign_keys_by_name(const std::optional<UuidBytes>& schema_oid,
                                                      const std::string& parent_relation_name) const
    {
        auto roid = lookup_object_oid(schema_oid, std::string("RELATION"), parent_relation_name);
        if (!roid)
            roid = lookup_object_oid(schema_oid, std::string("TABLE"), parent_relation_name);
        if (!roid)
            return {};
        return list_inbound_foreign_keys_by_parent(*roid);
    }

    std::unordered_map<std::string, std::string>
    CatalogManager::get_effective_column_defaults_by_name(
        const std::optional<UuidBytes>& schema_oid, const std::string& relation_name) const
    {
        std::unordered_map<std::string, std::string> out;
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_column_root_page)
            return out;
        // Column table layout with default_expr at attr index 6 (see bootstrap)
        TupleLayout col_layout{};
        col_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::Int64, 8, true, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::Int64, 8, true, false},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true}};
        FileMap fc(layout);
        fc.set_base_path(dir, base);
        auto col_rel = HeapRelation::open(std::move(fc), ps, *hi.sdb_column_root_page, col_layout);
        auto roid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!roid)
            roid = lookup_object_oid(schema_oid, std::string("TABLE"), relation_name);
        if (!roid)
            return out;
        // First pass: column-level default_expr
        {
            auto sc = col_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 7)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), roid->data(), 16) != 0)
                    continue;
                std::string name = row[2].is_null ? std::string() : row[2].bytes;
                if (!row[6].is_null && !name.empty())
                    out[name] = row[6].bytes;
            }
        }
        // Domain-level defaults are available in SDB$DOMAIN; map by domain_oid in column row[4]
        if (hi.sdb_domain_root_page) {
            TupleLayout dom_layout{};
            dom_layout.attrs = {
                {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
                {AttrType::Int64, 8, true, false},     {AttrType::Int64, 8, true, false},
                {AttrType::Int64, 8, true, false},     {AttrType::VarBytes, 0, false, true},
                {AttrType::VarBytes, 0, false, true},  {AttrType::Int64, 8, true, false},
                {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
            FileMap fd(layout);
            fd.set_base_path(dir, base);
            auto dom_rel =
                HeapRelation::open(std::move(fd), ps, *hi.sdb_domain_root_page, dom_layout);
            // Build domain oid -> default map
            std::unordered_map<std::string, std::string> dom_default;
            {
                auto sc = dom_rel.open_scan();
                std::vector<Value> row;
                ods::RowId rid{};
                while (sc.next(row, &rid)) {
                    if (row.size() < 10)
                        continue;
                    if (row[0].is_null || row[0].bytes.size() != 16)
                        continue;
                    std::string oid(reinterpret_cast<const char*>(row[0].bytes.data()), 16);
                    if (!row[8].is_null)
                        dom_default[oid] = row[8].bytes;
                }
            }
            // Second pass: fill missing column defaults from domain defaults
            auto sc = col_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 7)
                    continue;
                if (row[0].is_null || row[0].bytes.size() != 16)
                    continue;
                if (std::memcmp(row[0].bytes.data(), roid->data(), 16) != 0)
                    continue;
                std::string name = row[2].is_null ? std::string() : row[2].bytes;
                if (name.empty())
                    continue;
                if (!row[4].is_null && row[4].bytes.size() == 16 && out.find(name) == out.end()) {
                    std::string key(reinterpret_cast<const char*>(row[4].bytes.data()), 16);
                    auto it = dom_default.find(key);
                    if (it != dom_default.end())
                        out[name] = it->second;
                }
            }
        }
        return out;
    }
    bool CatalogManager::create_index_catalog(
        const std::optional<UuidBytes>& schema_oid, const UuidBytes& relation_oid,
        const std::string& name, const std::string& method,
        const std::vector<std::pair<std::string, std::string>>& keys, bool unique) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        HeaderManager hm(FileMap(layout), ps);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        hm = HeaderManager(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page)
            return false;
        // Insert into SDB$OBJECT(type=INDEX)
        TupleLayout object_layout{};
        object_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, false},
                               {AttrType::VarBytes, 0, false, true},
                               {AttrType::VarBytes, 0, false, false}};
        FileMap fmap_o(layout);
        fmap_o.set_base_path(dir, base);
        auto object_rel =
            HeapRelation::open(std::move(fmap_o), ps, *hi.sdb_object_root_page, object_layout);
        UuidBytes idx_oid{};
        std::hash<std::string> h;
        auto v =
            h(std::string(reinterpret_cast<const char*>(relation_oid.data()), relation_oid.size()) +
              std::string("::INDEX::") + name);
        std::memcpy(idx_oid.data(), &v, std::min(sizeof(v), idx_oid.size()));
        auto make_uuid_val = [](const UuidBytes& u) {
            Value v{};
            v.is_null = false;
            v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
            return v;
        };
        auto make_str = [](const std::string& s) {
            Value v{};
            v.is_null = false;
            v.bytes = s;
            return v;
        };
        auto make_opt_uuid = [](const std::optional<UuidBytes>& u) {
            Value v{};
            if (u) {
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(u->data()), u->size());
            } else
                v.is_null = true;
            return v;
        };
        object_rel.insert(
            {make_uuid_val(idx_oid), make_str("INDEX"), make_opt_uuid(schema_oid), make_str(name)});
        // Insert into SDB$INDEX
        if (hi.sdb_relation_root_page) {
            TupleLayout index_layout{};
            index_layout.attrs = {
                {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true},
                {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
                {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
                {AttrType::VarBytes, 0, false, true}};
            FileMap fmap_i(layout);
            fmap_i.set_base_path(dir, base);
            auto idx_rel =
                HeapRelation::open(std::move(fmap_i), ps, *hi.sdb_relation_root_page, index_layout);
            idx_rel.insert({make_uuid_val(idx_oid), make_uuid_val(relation_oid),
                            make_str(unique ? "TRUE" : "FALSE"), make_str(method), make_str(""),
                            make_str(""), make_str("")});
            // Create a physical empty B-Tree for this index and persist its root in stats JSON
            try {
                std::fprintf(stderr, "[INDEX CREATE] Creating physical B-Tree for index '%s'\\n",
                             name.c_str());
                FileMap fmap_idx(layout);
                fmap_idx.set_base_path(dir, base);
                BTreeIndex bt(std::move(fmap_idx), ps, unique);
                bt.create_empty();
                auto root_page = bt.root_page();
                std::fprintf(stderr, "[INDEX CREATE] B-Tree created, root_page=%u\\n", root_page);
                // Store root in SDB$STATS keyed by index OID (this is a non-const operation)
                const_cast<CatalogManager*>(this)->set_index_root(schema_oid, name, root_page);
                std::fprintf(stderr, "[INDEX CREATE] Root page stored for index '%s'\\n",
                             name.c_str());
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[INDEX CREATE] FAILED to create index '%s': %s\\n",
                             name.c_str(), e.what());
            } catch (...) {
                std::fprintf(stderr, "[INDEX CREATE] FAILED to create index '%s': unknown error\\n",
                             name.c_str());
            }
        }
        // Insert into SDB$INDEX_KEY
        if (hi.sdb_relation_root_page) {
            TupleLayout key_layout{};
            key_layout.attrs = {
                {AttrType::VarBytes, 0, false, false}, {AttrType::Int64, 8, true, false},
                {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
                {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
            FileMap fmap_k(layout);
            fmap_k.set_base_path(dir, base);
            auto key_rel =
                HeapRelation::open(std::move(fmap_k), ps, *hi.sdb_relation_root_page, key_layout);
            for (std::size_t i = 0; i < keys.size(); ++i) {
                Value idxv = make_uuid_val(idx_oid);
                Value pos{};
                pos.is_null = false;
                pos.u64 = static_cast<std::uint64_t>(i + 1);
                Value col = make_str(keys[i].first);
                Value expr{};
                expr.is_null = true;
                Value dirv = make_str(keys[i].second);
                Value coll{};
                coll.is_null = true;
                key_rel.insert({idxv, pos, col, expr, dirv, coll});
            }
        }
        return true;
    }

    bool CatalogManager::set_stats(const UuidBytes& object_oid, const std::string& json)
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        HeaderManager hm(FileMap(layout), ps);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        hm = HeaderManager(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_relation_root_page)
            return false;
        TupleLayout stats_layout{};
        stats_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                              {AttrType::VarBytes, 0, false, false}};
        FileMap fmap_s(layout);
        fmap_s.set_base_path(dir, base);
        auto stats_rel =
            HeapRelation::open(std::move(fmap_s), ps, *hi.sdb_relation_root_page, stats_layout);
        Value oidv{};
        oidv.is_null = false;
        oidv.bytes.assign(reinterpret_cast<const char*>(object_oid.data()), object_oid.size());
        Value txt{};
        txt.is_null = false;
        txt.bytes = json;
        stats_rel.insert({oidv, txt});
        return true;
    }

    std::optional<std::string> CatalogManager::get_stats(const UuidBytes& object_oid) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        HeaderManager hm(FileMap(layout), ps);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        hm = HeaderManager(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_relation_root_page)
            return std::nullopt;
        TupleLayout stats_layout{};
        stats_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                              {AttrType::VarBytes, 0, false, false}};
        FileMap fmap_s(layout);
        fmap_s.set_base_path(dir, base);
        auto stats_rel =
            HeapRelation::open(std::move(fmap_s), ps, *hi.sdb_relation_root_page, stats_layout);
        auto scan = stats_rel.open_scan();
        std::vector<Value> row;
        while (scan.next(row, nullptr)) {
            if (row.size() < 2)
                continue;
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            if (std::memcmp(row[0].bytes.data(), object_oid.data(), 16) != 0)
                continue;
            return row[1].is_null ? std::optional<std::string>{}
                                  : std::optional<std::string>{row[1].bytes};
        }
        return std::nullopt;
    }

    bool CatalogManager::alter_constraint_deferral(const std::optional<UuidBytes>& schema_oid,
                                                   const std::string& relation_name,
                                                   const std::string& constraint_name,
                                                   bool deferrable, bool initially_deferred) const
    {
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        HeaderManager hm(std::move(fmap), ps);
        auto hi = hm.read();
        if (!hi.sdb_object_root_page || !hi.sdb_relation_root_page)
            return false;
        // layouts
        TupleLayout obj_layout{};
        obj_layout.attrs = {{AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, false},
                            {AttrType::VarBytes, 0, false, true},
                            {AttrType::VarBytes, 0, false, false}};
        TupleLayout c_layout{};
        c_layout.attrs = {
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, false},
            {AttrType::VarBytes, 0, false, false}, {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true},
            {AttrType::VarBytes, 0, false, true},  {AttrType::VarBytes, 0, false, true}};
        FileMap fo1(layout);
        fo1.set_base_path(dir, base);
        auto obj_rel = HeapRelation::open(std::move(fo1), ps, *hi.sdb_object_root_page, obj_layout);
        FileMap fc1(layout);
        fc1.set_base_path(dir, base);
        auto cons_rel =
            HeapRelation::open(std::move(fc1), ps, *hi.sdb_relation_root_page, c_layout);
        // resolve relation oid
        auto rel_oid = lookup_object_oid(schema_oid, std::string("RELATION"), relation_name);
        if (!rel_oid)
            rel_oid = lookup_object_oid(schema_oid, std::string("TABLE"), relation_name);
        if (!rel_oid)
            return false;
        // find constraint oid by name in OBJECT
        std::optional<UuidBytes> coid;
        {
            auto sc = obj_rel.open_scan();
            std::vector<Value> row;
            ods::RowId rid{};
            while (sc.next(row, &rid)) {
                if (row.size() < 4)
                    continue;
                if (row[1].is_null || row[1].bytes != std::string("CONSTRAINT"))
                    continue;
                if (schema_oid && (row[2].is_null || row[2].bytes.size() != 16 ||
                                   std::memcmp(row[2].bytes.data(), schema_oid->data(), 16) != 0))
                    continue;
                if (row[3].is_null || row[3].bytes != constraint_name)
                    continue;
                UuidBytes tmp{};
                if (!row[0].is_null && row[0].bytes.size() == 16)
                    std::memcpy(tmp.data(), row[0].bytes.data(), 16);
                coid = tmp;
                break;
            }
        }
        if (!coid)
            return false;
        // scan CONSTRAINT rows and update flags for matching oid and relation
        auto sc = cons_rel.open_scan();
        std::vector<Value> row;
        ods::RowId rid{};
        while (sc.next(row, &rid)) {
            if (row.size() < 5)
                continue;
            if (row[0].is_null || row[0].bytes.size() != 16)
                continue;
            if (std::memcmp(row[0].bytes.data(), coid->data(), 16) != 0)
                continue;
            if (row[1].is_null || row[1].bytes.size() != 16)
                continue;
            if (std::memcmp(row[1].bytes.data(), rel_oid->data(), 16) != 0)
                continue;
            auto make_str = [&](const std::string& s) {
                Value v{};
                v.is_null = false;
                v.bytes = s;
                return v;
            };
            row[3] = make_str(deferrable ? "TRUE" : "FALSE");
            row[4] = make_str(initially_deferred ? "TRUE" : "FALSE");
            return cons_rel.update(rid, row, nullptr);
        }
        return false;
    }

    // ALTER TABLE column operations implementation
    bool CatalogManager::add_column(const std::optional<UuidBytes>& schema_oid,
                                    const std::string& relation_name,
                                    const std::string& column_definition) const
    {
        // Find the relation
        auto rel_oid = lookup_object_oid(schema_oid, "RELATION", relation_name);
        if (!rel_oid)
            return false;

        // Parse column definition (basic parsing for name and type)
        std::string colname = column_definition;
        auto sp = colname.find_first_of(" \t\n");
        if (sp != std::string::npos) {
            colname = colname.substr(0, sp);
        }

        // Get current max position
        auto cols = list_columns(*rel_oid);
        std::int64_t new_position = cols.empty() ? 0 : cols.back().first + 1;

        // Create the new column entry
        std::vector<std::pair<std::int64_t, std::string>> new_cols = {{new_position, colname}};
        return create_columns(*rel_oid, new_cols, {}, {});
    }

    bool CatalogManager::drop_column(const std::optional<UuidBytes>& schema_oid,
                                     const std::string& relation_name,
                                     const std::string& column_name) const
    {
        // Find the relation
        auto rel_oid = lookup_object_oid(schema_oid, "RELATION", relation_name);
        if (!rel_oid)
            return false;

        // TODO: Implement actual column removal from SDB$COLUMN
        // This is a placeholder implementation that would need to:
        // 1. Remove the column from SDB$COLUMN
        // 2. Update existing data to remove the column
        // 3. Update any dependent constraints/indexes
        std::fprintf(stderr, "[DROP COLUMN] Column %s from %s (placeholder)\n", column_name.c_str(),
                     relation_name.c_str());
        return true;
    }

    bool CatalogManager::alter_column_type(const std::optional<UuidBytes>& schema_oid,
                                           const std::string& relation_name,
                                           const std::string& column_name,
                                           const std::string& new_type) const
    {
        // TODO: Implement column type alteration
        // This requires updating SDB$COLUMN and potentially converting existing data
        std::fprintf(stderr, "[ALTER COLUMN TYPE] %s.%s to %s (placeholder)\n",
                     relation_name.c_str(), column_name.c_str(), new_type.c_str());
        return true;
    }

    bool CatalogManager::alter_column_default(const std::optional<UuidBytes>& schema_oid,
                                              const std::string& relation_name,
                                              const std::string& column_name,
                                              const std::string& default_value) const
    {
        // TODO: Implement column default alteration
        // This requires updating SDB$COLUMN default_value field
        std::fprintf(stderr, "[ALTER COLUMN DEFAULT] %s.%s = %s (placeholder)\n",
                     relation_name.c_str(), column_name.c_str(),
                     default_value.empty() ? "NULL" : default_value.c_str());
        return true;
    }

    bool CatalogManager::alter_column_not_null(const std::optional<UuidBytes>& schema_oid,
                                               const std::string& relation_name,
                                               const std::string& column_name, bool not_null) const
    {
        // TODO: Implement NOT NULL constraint alteration
        // This requires updating SDB$COLUMN not_null field and validating existing data
        std::fprintf(stderr, "[ALTER COLUMN NOT NULL] %s.%s = %s (placeholder)\n",
                     relation_name.c_str(), column_name.c_str(), not_null ? "NOT NULL" : "NULL");
        return true;
    }

    bool CatalogManager::rename_column(const std::optional<UuidBytes>& schema_oid,
                                       const std::string& relation_name,
                                       const std::string& old_name,
                                       const std::string& new_name) const
    {
        // TODO: Implement column renaming
        // This requires updating SDB$COLUMN name field and any dependent objects
        std::fprintf(stderr, "[RENAME COLUMN] %s.%s -> %s (placeholder)\n", relation_name.c_str(),
                     old_name.c_str(), new_name.c_str());
        return true;
    }

    bool CatalogManager::drop_index_by_name(const std::optional<UuidBytes>& schema_oid,
                                            const std::string& index_name) const
    {
        // Find the index object
        auto index_oid = lookup_object_oid(schema_oid, "INDEX", index_name);
        if (!index_oid) {
            return false; // Index not found
        }

        // TODO: Implement actual index removal:
        // 1. Remove from SDB$OBJECT
        // 2. Remove from SDB$INDEX
        // 3. Remove from SDB$INDEX_KEY
        // 4. Drop physical B-tree structure
        // 5. Clean up any index statistics

        std::fprintf(stderr, "[DROP INDEX] Removing index '%s' from catalog (placeholder)\n",
                     index_name.c_str());

        return true;
    }

    bool CatalogManager::create_routine(const std::optional<UuidBytes>& schema_oid,
                                        const std::string& name, const std::string& kind,
                                        const std::string& language, const std::string& security,
                                        const std::string& volatility, bool leakproof,
                                        bool returns_set,
                                        const std::vector<RoutineParamInfo>& params,
                                        const std::string& source_code) const
    {
        // Read header to get catalog root pages
        FileOptions fo{};
        fo.direct_io = false;
        auto fh = FileManager::open(db_path_ + ".seg0", fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t ps = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(ps, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);

        FileMap::Layout layout{};
        layout.page_size = ps;
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;

        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);

        HeaderManager hm(FileMap(layout), ps);
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        hm = HeaderManager(std::move(fmap), ps);
        auto hi = hm.read();

        if (!hi.sdb_object_root_page)
            return false;

        // Generate UUID for the routine
        auto uuid_bytes = uuid_v7_bytes();
        UuidBytes routine_oid{};
        std::copy(uuid_bytes.begin(), uuid_bytes.end(), routine_oid.begin());

        // Create object entry in SDB$OBJECT
        if (!create_object(routine_oid, kind, schema_oid, name)) {
            return false;
        }

        // Store source code in SDB$SOURCE if provided
        if (!source_code.empty()) {
            TupleLayout s_layout{};
            s_layout.attrs = {
                {AttrType::VarBytes, 0, false, false}, // object_oid
                {AttrType::VarBytes, 0, false, false}  // source
            };

            FileMap fmap_s(layout);
            fmap_s.set_base_path(dir, base);
            auto source_rel =
                HeapRelation::open(std::move(fmap_s), ps, *hi.sdb_source_root_page, s_layout);

            auto make_uuid_val = [](const UuidBytes& uuid) {
                Value v{};
                v.is_null = false;
                v.bytes.assign(reinterpret_cast<const char*>(uuid.data()), uuid.size());
                return v;
            };
            auto make_str = [](const std::string& s) {
                Value v{};
                v.is_null = false;
                v.bytes = s;
                return v;
            };

            source_rel.insert({make_uuid_val(routine_oid), make_str(source_code)});
        }

        std::fprintf(stderr, "[CREATE ROUTINE] Created %s '%s' successfully\n", kind.c_str(),
                     name.c_str());
        return true;
    }

    std::vector<CatalogManager::RoutineInfo>
    CatalogManager::list_routines(const std::optional<UuidBytes>& schema_oid) const
    {
        // Implementation placeholder - would need to query SDB$OBJECT and SDB$ROUTINE
        std::vector<RoutineInfo> routines;
        std::fprintf(stderr, "[LIST ROUTINES] Listing routines (placeholder)\n");
        return routines;
    }

    std::optional<CatalogManager::RoutineInfo>
    CatalogManager::get_routine_by_name(const std::optional<UuidBytes>& schema_oid,
                                        const std::string& name) const
    {
        // Implementation placeholder - would need to query catalog tables
        std::fprintf(stderr, "[GET ROUTINE] Getting routine '%s' (placeholder)\n", name.c_str());
        return std::nullopt;
    }

    std::vector<CatalogManager::RoutineParamInfo>
    CatalogManager::get_routine_params(const UuidBytes& routine_oid) const
    {
        // Implementation placeholder - would need to query SDB$ROUTINE_PARAM
        std::vector<RoutineParamInfo> params;
        std::fprintf(stderr, "[GET ROUTINE PARAMS] Getting parameters (placeholder)\n");
        return params;
    }

    bool CatalogManager::drop_routine_by_name(const std::optional<UuidBytes>& schema_oid,
                                              const std::string& name) const
    {
        // Implementation placeholder - would need to remove from all catalog tables
        std::fprintf(stderr, "[DROP ROUTINE] Dropping routine '%s' (placeholder)\n", name.c_str());
        return true;
    }

} // namespace scratchbird::engine
