#include "scratchbird/engine.h"

#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/catalog_bootstrap.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/config.h"
#include "scratchbird/engine/constants.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/generators.h"
#include "scratchbird/engine/header.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/ods.h"
#include "scratchbird/engine/system_oids.h"
#include "scratchbird/engine/txn.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace scratchbird
{

    struct Database {
        std::string path;
        std::uint32_t page_size{4096};
    };
    struct Session {
    };
    struct Transaction {
    };
    struct Statement {
    };

    static inline std::pair<std::string, std::string> split_path_base(const std::string& path)
    {
        auto slash = path.find_last_of('/');
        std::string dir = (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
        std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
        return {dir, base};
    }

    std::shared_ptr<Database> create_database(const std::string& path, const CreateDbOptions& opts,
                                              Status& status)
    {
        try {
            const auto& cfg = engine::get_engine_config();
            // File layout
            engine::FileMap::Layout layout{};
            layout.page_size = opts.page_size
                                   ? opts.page_size
                                   : (cfg.default_page_size ? cfg.default_page_size : 4096u);
            layout.pages_per_segment = 262144; // ~1GB segments at 4KB
            layout.options.direct_io = cfg.direct_io;
            if (cfg.prealloc_mb)
                layout.options.preallocate_bytes =
                    static_cast<std::size_t>(cfg.prealloc_mb) * 1024ull * 1024ull;
            auto [dir, base] = split_path_base(path);

            // Header
            engine::FileMap fmap_h(layout);
            fmap_h.set_base_path(dir, base);
            engine::HeaderManager hm(std::move(fmap_h), layout.page_size);
            engine::HeaderInfo hi{};
            hi.page_size = layout.page_size;
            hi.ods_major = 1;
            hi.ods_minor = 0;
            hi.roots = {/*space_catalog*/ 0, /*generators*/ 3, /*first_pip*/ 1, /*first_tip*/ 2};
            hi.page_cache = opts.page_cache;
            if (opts.sweep_interval)
                hi.sweep_interval = opts.sweep_interval;
            hi.reserve_space = opts.reserve_space;
            hi.seeded_schemas = {{"sys.catalog", 1}, {"sys.security", 2}, {"public", 3}};
            hi.catalog_major = 1;
            hi.catalog_minor = 0;
            hm.write_new(hi);

            // Allocation: init header/PIP
            engine::FileMap fmap_a(layout);
            fmap_a.set_base_path(dir, base);
            engine::Allocator alloc(&fmap_a, layout.page_size);
            alloc.init_new();

            // TIP seed
            engine::FileMap fmap_t(layout);
            fmap_t.set_base_path(dir, base);
            engine::TransactionManager tm(std::move(fmap_t), layout.page_size);
            tm.init_seed();

            // Generators page at page 3
            engine::FileMap fmap_g(layout);
            fmap_g.set_base_path(dir, base);
            engine::GeneratorsManager gm(std::move(fmap_g), layout.page_size, 3);
            gm.init_new();

            // Bootstrap catalog SQL sidecar
            engine::BootstrapOptions bopts{};
            std::string sql = engine::generate_catalog_bootstrap_sql(bopts);
            auto to_hex = [](const engine::UuidBytes& u) {
                static const char* hex = "0123456789abcdef";
                std::string s;
                s.resize(2 + 32);
                s[0] = '0';
                s[1] = 'x';
                for (size_t i = 0; i < 16; i++) {
                    s[2 + i * 2] = hex[(u[i] >> 4) & 0xF];
                    s[3 + i * 2] = hex[u[i] & 0xF];
                }
                return s;
            };
            auto replace_all = [](std::string& t, const std::string& from, const std::string& to) {
                size_t pos = 0;
                while ((pos = t.find(from, pos)) != std::string::npos) {
                    t.replace(pos, from.size(), to);
                    pos += to.size();
                }
            };
            replace_all(sql, "<SYS_CATALOG_UUID>", to_hex(engine::oid_sys_catalog_schema()));
            replace_all(sql, "<SYS_SECURITY_UUID>", to_hex(engine::oid_sys_security_schema()));
            replace_all(sql, "<SYS_MONITORING_UUID>", to_hex(engine::oid_sys_monitoring_schema()));
            replace_all(sql, "<PUBLIC_SCHEMA_UUID>", to_hex(engine::oid_public_schema()));
            replace_all(sql, "<SYSDBA_UUID>", to_hex(engine::oid_sysdba_user()));
            replace_all(sql, "<PUBLIC_ROLE_UUID>", to_hex(engine::oid_public_role()));
            {
                engine::FileOptions fo{};
                fo.direct_io = false;
                auto fh = engine::FileManager::open(dir + "/" + base + ".bootstrap.sql", fo,
                                                    /*create*/ true);
                engine::FileManager::pwrite(fh, sql.data(), sql.size(), 0);
                engine::FileManager::flush(fh);
            }

            // Create core catalog heap relations (SDB$SCHEMA and SDB$OBJECT) and seed rows
            std::uint32_t sdb_schema_root_page = 0;
            std::uint32_t sdb_object_root_page = 0;
            std::uint32_t sdb_relation_root_page = 0;
            std::uint32_t sdb_column_root_page = 0;
            {
                engine::FileMap fmap_c(layout);
                fmap_c.set_base_path(dir, base);
                engine::Allocator alloc_c(&fmap_c, layout.page_size);
                // Define minimal layouts using VarBytes to store UUIDs and names
                engine::TupleLayout schema_layout{};
                schema_layout.attrs = {
                    {engine::AttrType::VarBytes, 0, false, false}, // oid (UUID bytes)
                    {engine::AttrType::VarBytes, 0, false, true},  // parent_oid (nullable)
                    {engine::AttrType::VarBytes, 0, false, false}, // name
                    {engine::AttrType::VarBytes, 0, false, false}, // kind
                    {engine::AttrType::VarBytes, 0, false, true}   // path_cache (nullable)
                };
                // Allocate and write HeapRoot + first HeapData page for SDB$SCHEMA
                {
                    std::uint32_t root_p = alloc_c.allocate_free_page();
                    std::uint32_t data_p = alloc_c.allocate_free_page();
                    std::vector<std::uint8_t> root(layout.page_size, 0);
                    auto* ph = reinterpret_cast<engine::ods::PageHeader*>(root.data());
                    ph->page_size = layout.page_size;
                    ph->page_no = root_p;
                    ph->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapRoot);
                    engine::ods::HeapRootPayload hr{};
                    hr.version = 1;
                    hr.first_heap_page = data_p;
                    hr.last_heap_page = data_p;
                    hr.tuple_format_id = engine::compute_layout_format_id(schema_layout);
                    std::memcpy(root.data() + sizeof(engine::ods::PageHeader), &hr, sizeof hr);
                    ph->checksum = 0;
                    ph->checksum = engine::ods::crc32c(root.data(), root.size());
                    fmap_c.write_page(root_p, root.data());
                    std::vector<std::uint8_t> page(layout.page_size, 0);
                    auto* ph2 = reinterpret_cast<engine::ods::PageHeader*>(page.data());
                    ph2->page_size = layout.page_size;
                    ph2->page_no = data_p;
                    ph2->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapData);
                    engine::HeapPageCodec::init_heap_data_page(page);
                    ph2->checksum = 0;
                    ph2->checksum = engine::ods::crc32c(page.data(), page.size());
                    fmap_c.write_page(data_p, page.data());
                    sdb_schema_root_page = root_p;
                    // Open relation and insert seed schemas
                    engine::HeapRelation schema_rel(std::move(fmap_c), layout.page_size, root_p,
                                                    schema_layout);
                    auto make_uuid_bytes = [](const engine::UuidBytes& u) {
                        engine::Value v{};
                        v.is_null = false;
                        v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
                        return v;
                    };
                    auto make_str = [](const char* s) {
                        engine::Value v{};
                        v.is_null = false;
                        v.bytes = s;
                        return v;
                    };
                    auto make_null = []() {
                        engine::Value v{};
                        v.is_null = true;
                        return v;
                    };
                    // Seed <root>
                    schema_rel.insert({make_uuid_bytes(engine::oid_root_schema()), make_null(),
                                       make_str("<root>"), make_str("SYSTEM"), make_null()});
                    // sys.catalog (child of <root>)
                    schema_rel.insert({make_uuid_bytes(engine::oid_sys_catalog_schema()),
                                       make_uuid_bytes(engine::oid_root_schema()),
                                       make_str("sys.catalog"), make_str("SYSTEM"), make_null()});
                    // sys.security
                    {
                        engine::FileMap fmap_tmp(layout);
                        fmap_tmp.set_base_path(dir, base);
                        schema_rel = engine::HeapRelation::open(
                            std::move(fmap_tmp), layout.page_size, root_p, schema_layout);
                        schema_rel.insert({make_uuid_bytes(engine::oid_sys_security_schema()),
                                           make_uuid_bytes(engine::oid_root_schema()),
                                           make_str("sys.security"), make_str("SYSTEM"),
                                           make_null()});
                    }
                    // sys.monitoring
                    {
                        engine::FileMap fmap_tmp(layout);
                        fmap_tmp.set_base_path(dir, base);
                        schema_rel = engine::HeapRelation::open(
                            std::move(fmap_tmp), layout.page_size, root_p, schema_layout);
                        schema_rel.insert({make_uuid_bytes(engine::oid_sys_monitoring_schema()),
                                           make_uuid_bytes(engine::oid_root_schema()),
                                           make_str("sys.monitoring"), make_str("SYSTEM"),
                                           make_null()});
                    }
                    // public
                    {
                        engine::FileMap fmap_tmp(layout);
                        fmap_tmp.set_base_path(dir, base);
                        schema_rel = engine::HeapRelation::open(
                            std::move(fmap_tmp), layout.page_size, root_p, schema_layout);
                        schema_rel.insert({make_uuid_bytes(engine::oid_public_schema()),
                                           make_uuid_bytes(engine::oid_root_schema()),
                                           make_str("public"), make_str("USER"), make_null()});
                    }
                }

                // OBJECT table: oid, type, schema_oid, name
                engine::TupleLayout object_layout{};
                object_layout.attrs = {
                    {engine::AttrType::VarBytes, 0, false, false}, // oid
                    {engine::AttrType::VarBytes, 0, false, false}, // type
                    {engine::AttrType::VarBytes, 0, false, true},  // schema_oid (nullable)
                    {engine::AttrType::VarBytes, 0, false, false}  // name
                };
                {
                    engine::FileMap fmap_o(layout);
                    fmap_o.set_base_path(dir, base);
                    std::uint32_t root_p = alloc_c.allocate_free_page();
                    std::uint32_t data_p = alloc_c.allocate_free_page();
                    std::vector<std::uint8_t> root(layout.page_size, 0);
                    auto* ph = reinterpret_cast<engine::ods::PageHeader*>(root.data());
                    ph->page_size = layout.page_size;
                    ph->page_no = root_p;
                    ph->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapRoot);
                    engine::ods::HeapRootPayload hr{};
                    hr.version = 1;
                    hr.first_heap_page = data_p;
                    hr.last_heap_page = data_p;
                    hr.tuple_format_id = engine::compute_layout_format_id(object_layout);
                    std::memcpy(root.data() + sizeof(engine::ods::PageHeader), &hr, sizeof hr);
                    ph->checksum = 0;
                    ph->checksum = engine::ods::crc32c(root.data(), root.size());
                    fmap_o.write_page(root_p, root.data());
                    std::vector<std::uint8_t> page(layout.page_size, 0);
                    auto* ph2 = reinterpret_cast<engine::ods::PageHeader*>(page.data());
                    ph2->page_size = layout.page_size;
                    ph2->page_no = data_p;
                    ph2->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapData);
                    engine::HeapPageCodec::init_heap_data_page(page);
                    ph2->checksum = 0;
                    ph2->checksum = engine::ods::crc32c(page.data(), page.size());
                    fmap_o.write_page(data_p, page.data());
                    sdb_object_root_page = root_p;
                    engine::HeapRelation object_rel(std::move(fmap_o), layout.page_size, root_p,
                                                    object_layout);
                    auto make_uuid_bytes = [](const engine::UuidBytes& u) {
                        engine::Value v{};
                        v.is_null = false;
                        v.bytes.assign(reinterpret_cast<const char*>(u.data()), u.size());
                        return v;
                    };
                    auto make_str = [](const char* s) {
                        engine::Value v{};
                        v.is_null = false;
                        v.bytes = s;
                        return v;
                    };
                    auto make_null = []() {
                        engine::Value v{};
                        v.is_null = true;
                        return v;
                    };
                    // Four schema OBJECT rows (type=SCHEMA)
                    object_rel.insert({make_uuid_bytes(engine::oid_sys_catalog_schema()),
                                       make_str("SCHEMA"), make_null(), make_str("sys.catalog")});
                    {
                        engine::FileMap fmap_tmp(layout);
                        fmap_tmp.set_base_path(dir, base);
                        object_rel = engine::HeapRelation::open(
                            std::move(fmap_tmp), layout.page_size, root_p, object_layout);
                        object_rel.insert({make_uuid_bytes(engine::oid_sys_security_schema()),
                                           make_str("SCHEMA"), make_null(),
                                           make_str("sys.security")});
                    }
                    {
                        engine::FileMap fmap_tmp(layout);
                        fmap_tmp.set_base_path(dir, base);
                        object_rel = engine::HeapRelation::open(
                            std::move(fmap_tmp), layout.page_size, root_p, object_layout);
                        object_rel.insert({make_uuid_bytes(engine::oid_sys_monitoring_schema()),
                                           make_str("SCHEMA"), make_null(),
                                           make_str("sys.monitoring")});
                    }
                    {
                        engine::FileMap fmap_tmp(layout);
                        fmap_tmp.set_base_path(dir, base);
                        object_rel = engine::HeapRelation::open(
                            std::move(fmap_tmp), layout.page_size, root_p, object_layout);
                        object_rel.insert({make_uuid_bytes(engine::oid_public_schema()),
                                           make_str("SCHEMA"), make_null(), make_str("public")});
                    }
                }
            }

            // RELATION table: minimal (oid, heap_root_page)
            engine::TupleLayout relation_layout{};
            relation_layout.attrs = {
                {engine::AttrType::VarBytes, 0, false, false}, // oid
                {engine::AttrType::Int64, 8, true, false}      // heap_root_page
            };
            {
                engine::FileMap fmap_r(layout);
                fmap_r.set_base_path(dir, base);
                engine::Allocator alloc_r(&fmap_r, layout.page_size);
                std::uint32_t root_p = alloc_r.allocate_free_page();
                std::uint32_t data_p = alloc_r.allocate_free_page();
                std::vector<std::uint8_t> root(layout.page_size, 0);
                auto* ph = reinterpret_cast<engine::ods::PageHeader*>(root.data());
                ph->page_size = layout.page_size;
                ph->page_no = root_p;
                ph->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapRoot);
                engine::ods::HeapRootPayload hr{};
                hr.version = 1;
                hr.first_heap_page = data_p;
                hr.last_heap_page = data_p;
                hr.tuple_format_id = engine::compute_layout_format_id(relation_layout);
                std::memcpy(root.data() + sizeof(engine::ods::PageHeader), &hr, sizeof hr);
                ph->checksum = 0;
                ph->checksum = engine::ods::crc32c(root.data(), root.size());
                fmap_r.write_page(root_p, root.data());
                std::vector<std::uint8_t> page(layout.page_size, 0);
                auto* ph2 = reinterpret_cast<engine::ods::PageHeader*>(page.data());
                ph2->page_size = layout.page_size;
                ph2->page_no = data_p;
                ph2->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapData);
                engine::HeapPageCodec::init_heap_data_page(page);
                ph2->checksum = 0;
                ph2->checksum = engine::ods::crc32c(page.data(), page.size());
                fmap_r.write_page(data_p, page.data());
                sdb_relation_root_page = root_p;
            }

            // COLUMN table: minimal (relation_oid, position, name)
            engine::TupleLayout column_layout{};
            column_layout.attrs = {
                {engine::AttrType::VarBytes, 0, false, false}, // relation_oid
                {engine::AttrType::Int64, 8, true, false},     // position
                {engine::AttrType::VarBytes, 0, false, false}  // name
            };
            {
                engine::FileMap fmap_c2(layout);
                fmap_c2.set_base_path(dir, base);
                engine::Allocator alloc_c3(&fmap_c2, layout.page_size);
                std::uint32_t root_p = alloc_c3.allocate_free_page();
                std::uint32_t data_p = alloc_c3.allocate_free_page();
                std::vector<std::uint8_t> root(layout.page_size, 0);
                auto* ph = reinterpret_cast<engine::ods::PageHeader*>(root.data());
                ph->page_size = layout.page_size;
                ph->page_no = root_p;
                ph->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapRoot);
                engine::ods::HeapRootPayload hr{};
                hr.version = 1;
                hr.first_heap_page = data_p;
                hr.last_heap_page = data_p;
                hr.tuple_format_id = engine::compute_layout_format_id(column_layout);
                std::memcpy(root.data() + sizeof(engine::ods::PageHeader), &hr, sizeof hr);
                ph->checksum = 0;
                ph->checksum = engine::ods::crc32c(root.data(), root.size());
                fmap_c2.write_page(root_p, root.data());
                std::vector<std::uint8_t> page(layout.page_size, 0);
                auto* ph2 = reinterpret_cast<engine::ods::PageHeader*>(page.data());
                ph2->page_size = layout.page_size;
                ph2->page_no = data_p;
                ph2->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapData);
                engine::HeapPageCodec::init_heap_data_page(page);
                ph2->checksum = 0;
                ph2->checksum = engine::ods::crc32c(page.data(), page.size());
                fmap_c2.write_page(data_p, page.data());
                sdb_column_root_page = root_p;
            }

            // Allocate SOURCE and STATS roots now as they are frequently accessed
            // SOURCE: (object_oid, text, doc)
            {
                engine::TupleLayout source_layout{};
                source_layout.attrs = {{engine::AttrType::VarBytes, 0, false, false},
                                       {engine::AttrType::VarBytes, 0, false, false},
                                       {engine::AttrType::VarBytes, 0, false, true}};
                engine::FileMap fmap_s(layout);
                fmap_s.set_base_path(dir, base);
                engine::Allocator alloc_s(&fmap_s, layout.page_size);
                std::uint32_t root_p = alloc_s.allocate_free_page();
                std::uint32_t data_p = alloc_s.allocate_free_page();
                std::vector<std::uint8_t> root(layout.page_size, 0);
                auto* ph = reinterpret_cast<engine::ods::PageHeader*>(root.data());
                ph->page_size = layout.page_size;
                ph->page_no = root_p;
                ph->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapRoot);
                engine::ods::HeapRootPayload hr{};
                hr.version = 1;
                hr.first_heap_page = data_p;
                hr.last_heap_page = data_p;
                hr.tuple_format_id = engine::compute_layout_format_id(source_layout);
                std::memcpy(root.data() + sizeof(engine::ods::PageHeader), &hr, sizeof hr);
                ph->checksum = 0;
                ph->checksum = engine::ods::crc32c(root.data(), root.size());
                fmap_s.write_page(root_p, root.data());
                std::vector<std::uint8_t> page(layout.page_size, 0);
                auto* ph2 = reinterpret_cast<engine::ods::PageHeader*>(page.data());
                ph2->page_size = layout.page_size;
                ph2->page_no = data_p;
                ph2->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapData);
                engine::HeapPageCodec::init_heap_data_page(page);
                ph2->checksum = 0;
                ph2->checksum = engine::ods::crc32c(page.data(), page.size());
                fmap_s.write_page(data_p, page.data());
                // Persist SOURCE root in header
                engine::FileMap fmap_hdr2(layout);
                fmap_hdr2.set_base_path(dir, base);
                engine::HeaderManager hm_src(std::move(fmap_hdr2), layout.page_size);
                auto hi_src = hm.read();
                hi_src.sdb_source_root_page = root_p;
                hm_src.write_new(hi_src);
            }
            // STATS: (object_oid, stats)
            {
                engine::TupleLayout stats_layout{};
                stats_layout.attrs = {{engine::AttrType::VarBytes, 0, false, false},
                                      {engine::AttrType::VarBytes, 0, false, false}};
                engine::FileMap fmap_st(layout);
                fmap_st.set_base_path(dir, base);
                engine::Allocator alloc_st(&fmap_st, layout.page_size);
                std::uint32_t root_p = alloc_st.allocate_free_page();
                std::uint32_t data_p = alloc_st.allocate_free_page();
                std::vector<std::uint8_t> root(layout.page_size, 0);
                auto* ph = reinterpret_cast<engine::ods::PageHeader*>(root.data());
                ph->page_size = layout.page_size;
                ph->page_no = root_p;
                ph->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapRoot);
                engine::ods::HeapRootPayload hr{};
                hr.version = 1;
                hr.first_heap_page = data_p;
                hr.last_heap_page = data_p;
                hr.tuple_format_id = engine::compute_layout_format_id(stats_layout);
                std::memcpy(root.data() + sizeof(engine::ods::PageHeader), &hr, sizeof hr);
                ph->checksum = 0;
                ph->checksum = engine::ods::crc32c(root.data(), root.size());
                fmap_st.write_page(root_p, root.data());
                std::vector<std::uint8_t> page(layout.page_size, 0);
                auto* ph2 = reinterpret_cast<engine::ods::PageHeader*>(page.data());
                ph2->page_size = layout.page_size;
                ph2->page_no = data_p;
                ph2->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapData);
                engine::HeapPageCodec::init_heap_data_page(page);
                ph2->checksum = 0;
                ph2->checksum = engine::ods::crc32c(page.data(), page.size());
                fmap_st.write_page(data_p, page.data());
                // Persist STATS root in header
                engine::FileMap fmap_hdr2(layout);
                fmap_hdr2.set_base_path(dir, base);
                engine::HeaderManager hm_stats(std::move(fmap_hdr2), layout.page_size);
                auto hi_st = hm.read();
                hi_st.sdb_stats_root_page = root_p;
                hm_stats.write_new(hi_st);
            }

            // Update header with new catalog roots if available
            {
                engine::FileMap fmap_hdr2(layout);
                fmap_hdr2.set_base_path(dir, base);
                engine::HeaderManager hm3(std::move(fmap_hdr2), layout.page_size);
                auto hi3 = hm.read();
                hi3.catalog_major = 1;
                hi3.catalog_minor = 0;
                if (sdb_schema_root_page)
                    hi3.sdb_schema_root_page = sdb_schema_root_page;
                if (sdb_object_root_page)
                    hi3.sdb_object_root_page = sdb_object_root_page;
                if (sdb_relation_root_page)
                    hi3.sdb_relation_root_page = sdb_relation_root_page;
                if (sdb_column_root_page)
                    hi3.sdb_column_root_page = sdb_column_root_page;
                hm3.write_new(hi3);
            }

            // Flush seg0 explicitly (header write) via a short-lived handle
            {
                engine::FileOptions fo{};
                fo.direct_io = cfg.direct_io;
                auto fh = engine::FileManager::open(dir + "/" + base + ".seg0", fo, false);
                engine::FileManager::flush(fh);
            }

            // Mark header as bootstrapped (CatalogVersion and CatalogRoots clumplets) via
            // HeaderManager, preserving all discovered roots
            {
                engine::FileMap fmap_hdr(layout);
                fmap_hdr.set_base_path(dir, base);
                engine::HeaderManager hm2(std::move(fmap_hdr), layout.page_size);
                auto hi2 = hm.read();
                hi2.catalog_major = 1;
                hi2.catalog_minor = 0;
                if (sdb_schema_root_page)
                    hi2.sdb_schema_root_page = sdb_schema_root_page;
                if (sdb_object_root_page)
                    hi2.sdb_object_root_page = sdb_object_root_page;
                if (sdb_relation_root_page)
                    hi2.sdb_relation_root_page = sdb_relation_root_page;
                if (sdb_column_root_page)
                    hi2.sdb_column_root_page = sdb_column_root_page;
                // Preserve previously written SOURCE/STATS/DOMAIN roots if present
                // SOURCE
                {
                    engine::FileMap fmap_hdr3(layout);
                    fmap_hdr3.set_base_path(dir, base);
                    engine::HeaderManager hm_read(std::move(fmap_hdr3), layout.page_size);
                    auto hi_read = hm_read.read();
                    if (hi_read.sdb_source_root_page)
                        hi2.sdb_source_root_page = hi_read.sdb_source_root_page;
                    if (hi_read.sdb_domain_root_page)
                        hi2.sdb_domain_root_page = hi_read.sdb_domain_root_page;
                    if (hi_read.sdb_stats_root_page)
                        hi2.sdb_stats_root_page = hi_read.sdb_stats_root_page;
                }
                hm2.write_new(hi2);
            }

            // Materialize additional SDB$ relations (metadata rows) for core catalog tables
            {
                engine::CatalogManager cm(path);
                auto syscat = cm.lookup_schema_oid_by_name("sys.catalog");
                if (syscat) {
                    auto ensure_rel = [&](const std::string& name,
                                          const std::vector<std::string>& cols) {
                        if (!cm.lookup_object_oid(syscat, "RELATION", name)) {
                            cm.create_relation(*syscat, name, cols);
                        }
                    };
                    // Create SDB$DOMAIN relation and its heap root page
                    ensure_rel("SDB$DOMAIN",
                               {"oid", "base_type", "length", "precision", "scale", "charset",
                                "collate", "not_null", "default_expr", "check_expr"});
                    {
                        // Allocate root and first data page for SDB$DOMAIN and persist root in
                        // header
                        engine::FileMap fmap_d(layout);
                        fmap_d.set_base_path(dir, base);
                        engine::Allocator alloc_d(&fmap_d, layout.page_size);
                        std::uint32_t root_p = alloc_d.allocate_free_page();
                        std::uint32_t data_p = alloc_d.allocate_free_page();
                        std::vector<std::uint8_t> root(layout.page_size, 0);
                        auto* ph = reinterpret_cast<engine::ods::PageHeader*>(root.data());
                        ph->page_size = layout.page_size;
                        ph->page_no = root_p;
                        ph->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapRoot);
                        engine::TupleLayout domain_layout{};
                        domain_layout.attrs = {{engine::AttrType::VarBytes, 0, false, false},
                                               {engine::AttrType::VarBytes, 0, false, false},
                                               {engine::AttrType::Int64, 8, true, false},
                                               {engine::AttrType::Int64, 8, true, false},
                                               {engine::AttrType::Int64, 8, true, false},
                                               {engine::AttrType::VarBytes, 0, false, true},
                                               {engine::AttrType::VarBytes, 0, false, true},
                                               {engine::AttrType::Int64, 8, true, false},
                                               {engine::AttrType::VarBytes, 0, false, true},
                                               {engine::AttrType::VarBytes, 0, false, true}};
                        engine::ods::HeapRootPayload hr{};
                        hr.version = 1;
                        hr.first_heap_page = data_p;
                        hr.last_heap_page = data_p;
                        hr.tuple_format_id = engine::compute_layout_format_id(domain_layout);
                        std::memcpy(root.data() + sizeof(engine::ods::PageHeader), &hr, sizeof hr);
                        ph->checksum = 0;
                        ph->checksum = engine::ods::crc32c(root.data(), root.size());
                        fmap_d.write_page(root_p, root.data());
                        std::vector<std::uint8_t> page(layout.page_size, 0);
                        auto* ph2 = reinterpret_cast<engine::ods::PageHeader*>(page.data());
                        ph2->page_size = layout.page_size;
                        ph2->page_no = data_p;
                        ph2->type = static_cast<std::uint16_t>(engine::ods::PageType::HeapData);
                        engine::HeapPageCodec::init_heap_data_page(page);
                        ph2->checksum = 0;
                        ph2->checksum = engine::ods::crc32c(page.data(), page.size());
                        fmap_d.write_page(data_p, page.data());
                        // Write header clumplet update for DOMAIN root
                        engine::FileMap fmap_hdr2(layout);
                        fmap_hdr2.set_base_path(dir, base);
                        engine::HeaderManager hm4(std::move(fmap_hdr2), layout.page_size);
                        auto hi4 = hm.read();
                        hi4.sdb_domain_root_page = root_p;
                        hm4.write_new(hi4);
                    }
                    ensure_rel("SDB$INDEX", {"oid", "relation_oid", "unique", "method",
                                             "where_expr", "include_cols", "tablespace"});
                    ensure_rel("SDB$INDEX_KEY", {"index_oid", "position", "column_oid", "expr",
                                                 "direction", "collation"});
                    ensure_rel("SDB$CONSTRAINT", {"oid", "relation_oid", "type", "deferrable",
                                                  "initially_deferred", "check_expr", "index_oid",
                                                  "ref_relation_oid", "on_delete", "on_update"});
                    ensure_rel("SDB$CONSTRAINT_KEY",
                               {"constraint_oid", "position", "column_oid", "ref_column_oid"});
                    ensure_rel("SDB$ROUTINE", {"oid", "kind", "language", "security", "volatility",
                                               "leakproof", "returns_set"});
                    ensure_rel("SDB$ROUTINE_PARAM", {"routine_oid", "position", "name", "mode",
                                                     "domain_oid", "inline_type"});
                    ensure_rel("SDB$PACKAGE", {"oid"});
                    ensure_rel("SDB$PACKAGE_MEMBER", {"package_oid", "member_name", "routine_oid"});
                    ensure_rel("SDB$SEQUENCE", {"oid", "start_value", "increment_by", "min_value",
                                                "max_value", "cycle", "cache"});
                    ensure_rel("SDB$TRIGGER",
                               {"oid", "relation_oid", "timing", "events", "position"});
                    ensure_rel("SDB$EXCEPTION", {"oid", "message"});
                    ensure_rel("SDB$CHARSET", {"name", "description"});
                    ensure_rel("SDB$COLLATION", {"name", "base_charset", "deterministic"});
                    ensure_rel("SDB$GRANT", {"grantee_oid", "object_oid", "privilege",
                                             "grantor_oid", "grant_option", "columns"});
                    ensure_rel("SDB$DEPENDENCY",
                               {"from_oid", "to_oid", "kind", "detail_from", "detail_to"});
                    ensure_rel("SDB$SOURCE", {"object_oid", "text", "doc"});
                    ensure_rel("SDB$STATS", {"object_oid", "stats"});
                    ensure_rel("SDB$CATALOG_VERSION", {"major", "minor", "stamp"});
                    ensure_rel("SDB$MIGRATIONS", {"id", "applied_at", "text_hash"});
                }
            }

            auto db = std::make_shared<Database>();
            db->path = path;
            db->page_size = layout.page_size;
            status.code = StatusCode::Ok;
            status.message.clear();
            return db;
        } catch (const std::exception& ex) {
            status.code = StatusCode::Error;
            status.message = ex.what();
            return std::make_shared<Database>();
        }
    }

    std::shared_ptr<Database> open_database(const std::string& path, Status& status)
    {
        try {
            const auto& cfg = engine::get_engine_config();
            auto [dir, base] = split_path_base(path);
            std::string seg0 = dir + "/" + base + ".seg0";
            engine::FileOptions fo{};
            fo.direct_io = cfg.direct_io;
            auto fh = engine::FileManager::open(seg0, fo, /*create*/ false);
            // Read at least 4096 to inspect header
            std::vector<std::uint8_t> buf(4096, 0);
            engine::FileManager::pread(fh, buf.data(), buf.size(), 0);
            auto* hdr = reinterpret_cast<const engine::ods::PageHeader*>(buf.data());
            // Validate checksum by recomputing over declared page_size
            std::uint32_t page_size = hdr->page_size ? hdr->page_size : 4096u;
            // Validate page size allowed
            const auto& allowed = cfg.allowed_page_sizes;
            if (std::find(allowed.begin(), allowed.end(), page_size) == allowed.end()) {
                status.code = StatusCode::Error;
                status.message = "Unsupported page_size";
                return std::make_shared<Database>();
            }
            // Read full header page
            buf.assign(page_size, 0);
            engine::FileManager::pread(fh, buf.data(), buf.size(), 0);
            hdr = reinterpret_cast<const engine::ods::PageHeader*>(buf.data());
            // Check type and checksum
            if (cfg.checksum_policy != engine::ChecksumPolicy::Off) {
                std::vector<std::uint8_t> tmp = buf;
                reinterpret_cast<engine::ods::PageHeader*>(tmp.data())->checksum = 0;
                auto expect = engine::ods::crc32c(tmp.data(), tmp.size());
                if (hdr->checksum != expect) {
                    status.code = StatusCode::Error;
                    status.message = "Checksum mismatch at page 0";
                    return std::make_shared<Database>();
                }
            }
            // Validate ODS
            if (hdr->type != static_cast<std::uint16_t>(engine::ods::PageType::Header) ||
                hdr->page_size != page_size) {
                status.code = StatusCode::Error;
                status.message = "Invalid ODS header";
                return std::make_shared<Database>();
            }
            // Build FileMap and return handle
            engine::FileMap::Layout layout{};
            layout.page_size = page_size;
            layout.pages_per_segment = 262144;
            layout.options.direct_io = cfg.direct_io;
            if (cfg.prealloc_mb)
                layout.options.preallocate_bytes =
                    static_cast<std::size_t>(cfg.prealloc_mb) * 1024ull * 1024ull;
            engine::FileMap fmap(layout);
            fmap.set_base_path(dir, base);
            auto db = std::make_shared<Database>();
            db->path = path;
            db->page_size = page_size;
            status.code = StatusCode::Ok;
            status.message.clear();
            return db;
        } catch (const std::exception& ex) {
            status.code = StatusCode::Error;
            status.message = ex.what();
            return std::make_shared<Database>();
        }
    }

    void close_database(std::shared_ptr<Database>& db)
    {
        db.reset();
    }

    std::shared_ptr<Session> create_session(std::shared_ptr<Database>, Status& status)
    {
        status.code = StatusCode::NotImplemented;
        status.message = "create_session stub";
        return std::make_shared<Session>();
    }

    std::shared_ptr<Transaction> begin_transaction(std::shared_ptr<Session>, Status& status)
    {
        status.code = StatusCode::NotImplemented;
        status.message = "begin_transaction stub";
        return std::make_shared<Transaction>();
    }

    Status commit(std::shared_ptr<Transaction>)
    {
        return {StatusCode::NotImplemented, "commit stub"};
    }

    Status rollback(std::shared_ptr<Transaction>)
    {
        return {StatusCode::NotImplemented, "rollback stub"};
    }

    std::shared_ptr<Statement> prepare(std::shared_ptr<Session>, const std::string&, Status& status)
    {
        status.code = StatusCode::NotImplemented;
        status.message = "prepare stub";
        return std::make_shared<Statement>();
    }

    Status execute(std::shared_ptr<Statement>, const std::vector<std::string>&)
    {
        return {StatusCode::NotImplemented, "execute stub"};
    }

} // namespace scratchbird
