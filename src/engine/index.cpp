#include "scratchbird/engine/index.h"

#include <algorithm>
#include <sstream>

namespace scratchbird::engine
{

    static bool ieq(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
                return false;
        return true;
    }

    std::vector<ValidationMessage> validate_index_definition(const IndexCreateOptions& opts)
    {
        std::vector<ValidationMessage> msgs;
        if (opts.index_name.empty())
            msgs.push_back({true, "Index name is required (e.g., 'idx_user_email')"});
        if (opts.relation_name.empty())
            msgs.push_back({true, "Relation name is required (table or view to index)"});
        if (opts.keys.empty())
            msgs.push_back({true, "At least one index key column is required"});
        // Method-specific checks
        switch (opts.method) {
        case IndexMethod::BTree:
            // Allow expressions, directions, collation
            break;
        case IndexMethod::Hash:
            // Typically single column, no expression, no DESC
            if (opts.keys.size() != 1)
                msgs.push_back({true, "HASH index supports exactly one key"});
            if (!opts.keys[0].expression.empty())
                msgs.push_back({true, "HASH index does not support expression keys"});
            if (!opts.where_predicate.empty())
                msgs.push_back({true, "HASH index does not support partial predicate"});
            break;
        case IndexMethod::Gin:
            // Requires expression or tsvector-like/text array; we accept stub validation
            if (opts.keys.size() != 1)
                msgs.push_back({true, "GIN index supports exactly one key"});
            break;
        case IndexMethod::RTree:
            // Spatial: requires one geometry column; no expressions
            if (opts.keys.size() != 1)
                msgs.push_back({true, "RTREE index supports exactly one key"});
            if (!opts.keys[0].expression.empty())
                msgs.push_back({true, "RTREE index does not support expression keys"});
            break;
        case IndexMethod::Bitmap:
            // Bitmap often supports multiple columns but no expressions
            for (const auto& k : opts.keys)
                if (!k.expression.empty())
                    msgs.push_back({true, "BITMAP index does not support expression keys"});
            break;
        case IndexMethod::PartialHash:
            // Partial Hash: single key + predicate
            if (opts.keys.size() != 1)
                msgs.push_back({true, "PARTIAL HASH index supports exactly one key"});
            if (opts.where_predicate.empty())
                msgs.push_back({true, "PARTIAL HASH requires WHERE predicate"});
            break;
        case IndexMethod::LSMTree:
            // LSM-Tree: optimized for write-heavy workloads
            if (opts.keys.empty())
                msgs.push_back({true, "LSM-Tree index requires at least one key"});
            if (!opts.compaction_strategy.empty() && opts.compaction_strategy != "SIZE_TIERED" &&
                opts.compaction_strategy != "LEVELED")
                msgs.push_back(
                    {true, "Invalid compaction strategy; expected SIZE_TIERED or LEVELED"});
            break;
        case IndexMethod::Columnstore:
            // Columnstore: analytical workloads, compression support
            if (opts.keys.empty())
                msgs.push_back({true, "Columnstore index requires at least one key column"});
            if (opts.unique)
                msgs.push_back(
                    {true, "Columnstore index cannot be unique (designed for analytics)"});
            break;
        case IndexMethod::TTL:
            msgs.push_back({true, "TTL index method not yet implemented"});
            break;
        }
        // Direction and collation check
        for (const auto& k : opts.keys) {
            if (!k.direction.empty() && !ieq(k.direction, "ASC") && !ieq(k.direction, "DESC"))
                msgs.push_back({true, "Invalid key direction; expected ASC or DESC"});
        }
        return msgs;
    }

    std::string format_index_definition(const IndexCreateOptions& opts)
    {
        std::ostringstream ss;
        ss << (opts.unique ? "UNIQUE " : "") << "INDEX " << opts.index_name << " ON "
           << opts.relation_name << " (";
        for (size_t i = 0; i < opts.keys.size(); ++i) {
            const auto& k = opts.keys[i];
            if (!k.expression.empty())
                ss << k.expression;
            else
                ss << k.column_name;
            if (!k.direction.empty())
                ss << " " << k.direction;
            if (!k.collation.empty())
                ss << " COLLATE " << k.collation;
            if (i + 1 < opts.keys.size())
                ss << ", ";
        }
        ss << ")";
        switch (opts.method) {
        case IndexMethod::BTree:
            break;
        case IndexMethod::Hash:
            ss << " USING HASH";
            break;
        case IndexMethod::Gin:
            ss << " USING GIN";
            break;
        case IndexMethod::RTree:
            ss << " USING RTREE";
            break;
        case IndexMethod::Bitmap:
            ss << " USING BITMAP";
            break;
        case IndexMethod::PartialHash:
            ss << " USING PARTIAL_HASH";
            break;
        case IndexMethod::LSMTree:
            ss << " USING LSM";
            if (!opts.compaction_strategy.empty()) {
                ss << " WITH (compaction_strategy=" << opts.compaction_strategy << ")";
            }
            break;
        case IndexMethod::Columnstore:
            ss << " USING COLUMNSTORE";
            if (!opts.compression_algorithm.empty()) {
                ss << " WITH (compression=" << opts.compression_algorithm << ")";
            }
            break;
        case IndexMethod::TTL:
            ss << " USING TTL";
            if (!opts.ttl_expire_column.empty() && !opts.ttl_interval.empty()) {
                ss << " EXPIRE AFTER " << opts.ttl_interval << " ON " << opts.ttl_expire_column;
            }
            break;
        }
        if (!opts.include_columns.empty()) {
            ss << " INCLUDE (";
            for (size_t i = 0; i < opts.include_columns.size(); ++i) {
                ss << opts.include_columns[i];
                if (i + 1 < opts.include_columns.size())
                    ss << ", ";
            }
            ss << ")";
        }
        if (!opts.where_predicate.empty())
            ss << " WHERE " << opts.where_predicate;
        if (!opts.tablespace.empty())
            ss << " TABLESPACE " << opts.tablespace;
        return ss.str();
    }

} // namespace scratchbird::engine
