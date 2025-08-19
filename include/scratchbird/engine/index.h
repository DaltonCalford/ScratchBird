#ifndef SCRATCHBIRD_ENGINE_INDEX_H
#define SCRATCHBIRD_ENGINE_INDEX_H

#include <string>
#include <vector>

namespace scratchbird::engine
{

    enum class IndexMethod { BTree, Hash, Gin, RTree, Bitmap, PartialHash };

    struct IndexKeySpec {
        std::string column_name; // optional if expr is set
        std::string expression;  // non-empty for expression index
        std::string direction;   // ASC|DESC
        std::string collation;   // optional
    };

    struct IndexCreateOptions {
        std::string index_name;
        std::string relation_name;
        bool unique{false};
        IndexMethod method{IndexMethod::BTree};
        std::vector<IndexKeySpec> keys;
        std::vector<std::string> include_columns; // covering
        std::string where_predicate;              // partial index
        std::string tablespace;                   // optional
        bool active{true};
    };

    struct ValidationMessage {
        bool error{false};
        std::string text;
    };

    // Validate index definition for method-specific constraints. Returns messages; any with
    // error=true block creation.
    std::vector<ValidationMessage> validate_index_definition(const IndexCreateOptions& opts);

    // Render method and options to a normalized summary string for diagnostics/logging.
    std::string format_index_definition(const IndexCreateOptions& opts);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_H
