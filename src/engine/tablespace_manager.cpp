#include "scratchbird/engine/tablespace_manager.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/config.h"
#include "scratchbird/engine/header.h"

#include <algorithm>
#include <cstring>

namespace scratchbird::engine
{

    static inline std::string to_upper_copy(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return char(std::toupper(c)); });
        return s;
    }

    bool TablespaceManager::load_from_catalog()
    {
        spaces_by_id_.clear();
        spaces_by_name_.clear();
        const auto& cfg = get_engine_config();
        if (!cfg.tablespaces_enabled)
            return true; // feature disabled: treat as empty and default implied

        CatalogManager cm(db_path_);
        // Ensure catalog has been bootstrapped (on demand backfill via CatalogManager)
        cm.bootstrap_if_needed();

        // For the initial phase: best-effort read via plain SDB$OBJECT listing until explicit
        // SDB$TABLESPACE relation is materialized by bootstrap. Fallback to synthetic default.
        // Try to locate a TABLESPACE object named SDB$DEFAULT
        auto oid = cm.lookup_object_oid(std::nullopt, std::string("TABLESPACE"),
                                        std::string("SDB$DEFAULT"));
        if (!oid) {
            // No explicit tablespace objects yet; synthesize default
            ensure_default();
            return true;
        }
        // Minimal in-memory entry
        TablespaceSpec def{};
        def.id = 1;
        def.name = "SDB$DEFAULT";
        def.state = TablespaceState::Online;
        spaces_by_id_[def.id] = def;
        spaces_by_name_[to_upper_copy(def.name)] = def;
        return true;
    }

    bool TablespaceManager::persist_to_catalog(const TablespaceSpec& spec) const
    {
        const auto& cfg = get_engine_config();
        if (!cfg.tablespaces_enabled)
            return true;
        CatalogManager cm(db_path_);
        auto syscat = cm.lookup_schema_oid_by_name("sys.catalog");
        if (!syscat)
            return false;
        // Create OBJECT row type=TABLESPACE if missing
        if (!cm.lookup_object_oid(syscat, std::string("TABLESPACE"), spec.name)) {
            // Deterministic oid via name
            UuidBytes oid{};
            std::hash<std::string> h;
            auto v = h(std::string("TABLESPACE::") + spec.name);
            std::memcpy(oid.data(), &v, std::min(sizeof(v), oid.size()));
            // Insert into OBJECT
            cm.create_object(oid, std::string("TABLESPACE"), syscat, spec.name);
        }
        // Datafile and usage persistence to be implemented in later milestones.
        return true;
    }

    bool TablespaceManager::ensure_default()
    {
        // Create synthetic default entry in-memory, ensure OBJECT catalog row exists
        TablespaceSpec def{};
        def.id = 1;
        def.name = "SDB$DEFAULT";
        def.state = TablespaceState::Online;
        upsert(def);
        return persist_to_catalog(def);
    }

    std::optional<TablespaceSpec> TablespaceManager::get_by_name(const std::string& name) const
    {
        auto it = spaces_by_name_.find(to_upper_copy(name));
        if (it == spaces_by_name_.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<TablespaceSpec> TablespaceManager::get_by_id(std::uint64_t id) const
    {
        auto it = spaces_by_id_.find(id);
        if (it == spaces_by_id_.end())
            return std::nullopt;
        return it->second;
    }

    std::pair<std::uint64_t, std::uint64_t>
    TablespaceManager::compute_allocation_range(const std::string& tablespace_name,
                                                std::optional<std::uint64_t> file_id_hint) const
    {
        // For Phase 14 Milestone A, the database uses a single FileMap with logical pages. We map
        // default tablespace to the whole range starting after bootstrap-reserved pages.
        (void)file_id_hint;
        auto ts = get_by_name(tablespace_name);
        if (!ts)
            return {2, UINT64_C(1) << 62};
        // Skip header(0), pip(1); allow allocator to choose starting at page 2.
        return {2, UINT64_C(1) << 62};
    }

    void TablespaceManager::upsert(const TablespaceSpec& spec)
    {
        spaces_by_id_[spec.id] = spec;
        spaces_by_name_[to_upper_copy(spec.name)] = spec;
    }

} // namespace scratchbird::engine
