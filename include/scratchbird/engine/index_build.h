#ifndef SCRATCHBIRD_ENGINE_INDEX_BUILD_H
#define SCRATCHBIRD_ENGINE_INDEX_BUILD_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    struct IndexDefinition {
        std::string index_name;
        std::string relation_name;
        bool unique{false};
        std::size_t include_count{0};
        std::string method{"BTREE"};
        std::string predicate_raw; // for partial indexes
    };

    struct IndexBuildOptions {
        std::uint32_t page_size{4096};
        bool online{false};
    };

    struct IndexBuildResult {
        std::uint32_t root_page{0};
        std::uint32_t height{0};
        std::uint32_t leaf_pages{0};
        std::uint32_t branch_pages{0};
        std::uint64_t key_count{0};
        std::string meta_path; // sidecar with root/page_size
        std::string base_path; // index file base path (segment files use this)
    };

    // Lightweight offline builder and validator. Intended for Phase H scaffolding and tests.
    class IndexBuildManager
    {
      public:
        // Build an index offline from sorted or unsorted keys. Creates files at base_path (e.g.,
        // /tmp/idx_X). Writes a sidecar meta file base_path + ".meta" storing root_page and
        // page_size.
        static IndexBuildResult
        build_offline(const IndexDefinition& def,
                      const std::vector<std::pair<std::string, std::uint64_t>>& keys,
                      const IndexBuildOptions& opts, const std::string& base_path);

        // Validate index structure by scanning leaves and verifying ordering and sibling links.
        // Returns true on success; error contains reason on failure.
        static bool validate_btree(const std::string& base_path, std::string& error);
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_BUILD_H
