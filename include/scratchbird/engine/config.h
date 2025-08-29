#ifndef SCRATCHBIRD_ENGINE_CONFIG_H
#define SCRATCHBIRD_ENGINE_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    enum class ChecksumPolicy { Off, Crc32c, VerifyOnRead };
    enum class FsyncPolicy { Always, Group, OsDefault };

    struct EngineConfig {
        std::vector<std::uint32_t> allowed_page_sizes; // bytes
        std::uint32_t default_page_size{4096};
        std::uint32_t prealloc_mb{0};
        bool direct_io{false};
        ChecksumPolicy checksum_policy{ChecksumPolicy::Crc32c};
        FsyncPolicy fsync_policy{FsyncPolicy::Always};
        // Performance hints
        bool prefetch_on_alloc{false};
        std::uint32_t prefetch_horizon_pages{128};
        // Bootstrap control
        bool bootstrap_execute{false};
        // Tablespace management (Phase 14)
        bool tablespaces_enabled{true};
        // Phase 18 feature flags (defaults chosen for safety/back-compat)
        bool enable_partition_pruning{true};      // safe when partitions are registered
        bool enable_partition_wise_ops{false};    // advanced, default off
        bool enable_materialized_views{true};     // foundation enabled; no-op unless used
        bool enable_mv_incremental{false};        // advanced, default off
        bool enable_mv_concurrent_refresh{false}; // advanced, default off
        bool enable_query_rewrite{false};         // advanced, default off
        bool enable_global_indexes{false};        // advanced, default off
    };

    // Load configuration from environment or file path indicated by SB_CONFIG.
    // Simple key=value format, lines starting with '#' are comments.
    // Keys:
    //  - allowed_page_sizes: comma-separated ints (bytes)
    //  - default_page_size: int
    //  - prealloc_mb: int
    //  - direct_io: true|false|1|0|on|off
    //  - checksum_policy: off|crc32c|verify_on_read
    //  - fsync_policy: always|group|os-default
    //  - prefetch_on_alloc: true|false
    //  - prefetch_horizon_pages: int (number of pages to hint will-need starting at allocated page)
    //  - bootstrap_execute: bool (execute bootstrap SQL during create; default false writes sidecar
    //  only)
    //  - tablespaces_enabled: bool (enable tablespace management functionality; default true)
    //  - enable_partition_pruning: bool
    //  - enable_partition_wise_ops: bool
    //  - enable_materialized_views: bool
    //  - enable_mv_incremental: bool
    //  - enable_mv_concurrent_refresh: bool
    //  - enable_query_rewrite: bool
    //  - enable_global_indexes: bool
    EngineConfig load_engine_config();
    const EngineConfig& get_engine_config();

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_CONFIG_H
