#ifndef SCRATCHBIRD_ENGINE_TABLESPACE_MANAGER_H
#define SCRATCHBIRD_ENGINE_TABLESPACE_MANAGER_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    enum class TablespaceState : std::uint8_t { Online = 1, Offline = 2 };

    struct DatafileSpec {
        std::uint64_t id{0};
        std::string path;
        std::uint64_t size_bytes{0};
        bool auto_extend{false};
        std::uint64_t max_size_bytes{0};
        std::string state; // AVAILABLE/UNAVAILABLE
    };

    struct TablespaceSpec {
        std::uint64_t id{0};
        std::string name;
        TablespaceState state{TablespaceState::Online};
        std::string options_json; // arbitrary options serialized for now
        std::vector<DatafileSpec> files;
    };

    // Minimal persistence contract: relies on catalog relations SDB$TABLESPACE, SDB$DATAFILE,
    // SDB$TABLESPACE_USAGE. The manager exposes an in-memory view and simple allocation hints.
    class TablespaceManager
    {
      public:
        explicit TablespaceManager(std::string db_path, std::uint32_t page_size)
            : db_path_(std::move(db_path)), page_size_(page_size)
        {
        }

        bool load_from_catalog();
        bool persist_to_catalog(const TablespaceSpec& spec) const;

        // Ensure default tablespace exists (SDB$DEFAULT) with implicit seg files
        bool ensure_default();

        // Lookup by name/id
        std::optional<TablespaceSpec> get_by_name(const std::string& name) const;
        std::optional<TablespaceSpec> get_by_id(std::uint64_t id) const;

        // Allocation hint for a given tablespace/file: returns an inclusive logical page range
        // [start,end] for allocator to try first when tablespace maps to logical regions.
        // For the initial implementation that uses a single FileMap, we approximate ranges by
        // segment stripes: each datafile corresponds to a contiguous stripe of segments.
        std::pair<std::uint64_t, std::uint64_t>
        compute_allocation_range(const std::string& tablespace_name,
                                 std::optional<std::uint64_t> file_id_hint) const;

        // Upsert tablespace and its files in memory
        void upsert(const TablespaceSpec& spec);

        const std::unordered_map<std::string, TablespaceSpec>& by_name() const
        {
            return spaces_by_name_;
        }

      private:
        std::string db_path_;
        std::uint32_t page_size_{4096};
        std::unordered_map<std::string, TablespaceSpec> spaces_by_name_;
        std::unordered_map<std::uint64_t, TablespaceSpec> spaces_by_id_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_TABLESPACE_MANAGER_H
