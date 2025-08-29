#include "scratchbird/engine/config.h"

#include "scratchbird/engine/constants.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace scratchbird::engine
{

    namespace
    {
        static inline std::string trim(std::string s)
        {
            auto not_space = [](int ch) { return !std::isspace(ch); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
            s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
            return s;
        }
        static inline bool parse_bool(const std::string& v)
        {
            std::string l;
            l.resize(v.size());
            std::transform(v.begin(), v.end(), l.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            return (l == "1" || l == "true" || l == "on" || l == "yes");
        }
        static inline ChecksumPolicy from_checksum(const std::string& v)
        {
            std::string l;
            l.resize(v.size());
            std::transform(v.begin(), v.end(), l.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            if (l == "off")
                return ChecksumPolicy::Off;
            if (l == "verify_on_read" || l == "verify-on-read")
                return ChecksumPolicy::VerifyOnRead;
            return ChecksumPolicy::Crc32c;
        }
        static inline FsyncPolicy from_fsync(const std::string& v)
        {
            std::string l;
            l.resize(v.size());
            std::transform(v.begin(), v.end(), l.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            if (l == "group")
                return FsyncPolicy::Group;
            if (l == "os-default" || l == "os_default")
                return FsyncPolicy::OsDefault;
            return FsyncPolicy::Always;
        }
    } // namespace

    EngineConfig load_engine_config()
    {
        EngineConfig cfg{};
        // defaults
        cfg.allowed_page_sizes.assign(kAllowedPageSizesBytes.begin(), kAllowedPageSizesBytes.end());
        cfg.default_page_size = 4096u;
        cfg.prealloc_mb = 0;
        cfg.direct_io = false;
        cfg.checksum_policy = ChecksumPolicy::Crc32c;
        cfg.fsync_policy = FsyncPolicy::Always;
        cfg.prefetch_on_alloc = false;
        cfg.prefetch_horizon_pages = 128;
        cfg.bootstrap_execute = false;

        const char* path = std::getenv("SB_CONFIG");
        if (path && *path) {
            std::ifstream in(path);
            std::string line;
            while (std::getline(in, line)) {
                auto hash = line.find('#');
                if (hash != std::string::npos)
                    line = line.substr(0, hash);
                line = trim(line);
                if (line.empty())
                    continue;
                auto eq = line.find('=');
                if (eq == std::string::npos)
                    continue;
                std::string key = trim(line.substr(0, eq));
                std::string val = trim(line.substr(eq + 1));
                // normalize key lower
                std::transform(key.begin(), key.end(), key.begin(),
                               [](unsigned char c) { return char(std::tolower(c)); });
                if (key == "allowed_page_sizes") {
                    cfg.allowed_page_sizes.clear();
                    std::stringstream ss(val);
                    std::string tok;
                    while (std::getline(ss, tok, ',')) {
                        tok = trim(tok);
                        if (tok.empty())
                            continue;
                        std::uint32_t x =
                            static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10));
                        if (x)
                            cfg.allowed_page_sizes.push_back(x);
                    }
                    if (cfg.allowed_page_sizes.empty()) {
                        cfg.allowed_page_sizes.assign(kAllowedPageSizesBytes.begin(),
                                                      kAllowedPageSizesBytes.end());
                    }
                } else if (key == "default_page_size") {
                    cfg.default_page_size =
                        static_cast<std::uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
                } else if (key == "prealloc_mb") {
                    cfg.prealloc_mb =
                        static_cast<std::uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
                } else if (key == "direct_io") {
                    cfg.direct_io = parse_bool(val);
                } else if (key == "checksum_policy") {
                    cfg.checksum_policy = from_checksum(val);
                } else if (key == "fsync_policy") {
                    cfg.fsync_policy = from_fsync(val);
                } else if (key == "prefetch_on_alloc") {
                    cfg.prefetch_on_alloc = parse_bool(val);
                } else if (key == "prefetch_horizon_pages") {
                    cfg.prefetch_horizon_pages =
                        static_cast<std::uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
                } else if (key == "bootstrap_execute") {
                    cfg.bootstrap_execute = parse_bool(val);
                } else if (key == "tablespaces_enabled") {
                    cfg.tablespaces_enabled = parse_bool(val);
                } else if (key == "enable_partition_pruning") {
                    cfg.enable_partition_pruning = parse_bool(val);
                } else if (key == "enable_partition_wise_ops") {
                    cfg.enable_partition_wise_ops = parse_bool(val);
                } else if (key == "enable_materialized_views") {
                    cfg.enable_materialized_views = parse_bool(val);
                } else if (key == "enable_mv_incremental") {
                    cfg.enable_mv_incremental = parse_bool(val);
                } else if (key == "enable_mv_concurrent_refresh") {
                    cfg.enable_mv_concurrent_refresh = parse_bool(val);
                } else if (key == "enable_query_rewrite") {
                    cfg.enable_query_rewrite = parse_bool(val);
                } else if (key == "enable_global_indexes") {
                    cfg.enable_global_indexes = parse_bool(val);
                }
            }
        }
        return cfg;
    }

    const EngineConfig& get_engine_config()
    {
        static EngineConfig cfg = load_engine_config();
        return cfg;
    }

} // namespace scratchbird::engine
