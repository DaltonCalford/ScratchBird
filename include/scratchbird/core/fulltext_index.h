#pragma once

#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/gin_tsvector_ops.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/tid.h"
#include <memory>
#include <vector>

namespace scratchbird::core
{

// Forward declarations
class Database;

/**
 * @brief Full-Text Search Index
 *
 * This class provides a specialized index for full-text search using tsvector
 * and tsquery types. It's implemented as a wrapper around GIN (Generalized
 * Inverted Index) with the tsvector operator class.
 *
 * Architecture:
 * - Backend: GIN index with GINTSVectorOps operator class
 * - Indexed type: TSVector (text search vector with lexemes and positions)
 * - Query type: TSQuery (Boolean expressions over lexemes)
 * - Operator: @@ (text search match)
 *
 * Firebird MGA Compliance:
 * - Uses TIP-based visibility (inherited from GIN)
 * - TransactionId current_xid parameters (NOT Snapshot*)
 * - xmin/xmax tracking for multi-version concurrency
 *
 * Usage:
 *   // Create index
 *   uint32_t root_page;
 *   FullTextIndex::create(db, index_uuid, table_uuid, column_ids, &root_page, ctx);
 *
 *   // Open index
 *   auto index = FullTextIndex::open(db, index_uuid, table_uuid, column_ids, root_page, ctx);
 *
 *   // Insert tsvector value
 *   TSVector vec = TSVector::fromString("'cat':1,3 'dog':2");
 *   std::vector<uint8_t> data = vec.toBinary();
 *   index->insert(data.data(), data.size(), tid, ctx);
 *
 *   // Search with tsquery
 *   TSQuery query = TSQuery::fromString("cat & dog");
 *   std::vector<TID> results = index->search(query, current_xid, ctx);
 */
class FullTextIndex
{
public:
    /**
     * @brief Constructor
     *
     * @param db Database instance
     * @param index_uuid UUID of the index
     * @param table_uuid UUID of the table being indexed
     * @param column_ids Columns being indexed (typically one tsvector column)
     * @param gin_index Underlying GIN index
     */
    FullTextIndex(Database* db,
                  const ID& index_uuid,
                  const ID& table_uuid,
                  const std::vector<ID>& column_ids,
                  std::unique_ptr<GinIndex> gin_index);

    /**
     * @brief Create a new full-text index
     *
     * Allocates a GIN index configured for tsvector/tsquery operations.
     *
     * @param db Database instance
     * @param index_uuid UUID for the new index
     * @param table_uuid UUID of the table being indexed
     * @param column_ids Columns being indexed
     * @param root_page_out Output: root page number
     * @param ctx Error context
     * @return Status code
     */
    static Status create(Database* db,
                        const ID& index_uuid,
                        const ID& table_uuid,
                        const std::vector<ID>& column_ids,
                        uint32_t* root_page_out,
                        ErrorContext* ctx = nullptr);

    /**
     * @brief Open an existing full-text index
     *
     * @param db Database instance
     * @param index_uuid UUID of the index
     * @param table_uuid UUID of the table being indexed
     * @param column_ids Columns being indexed
     * @param root_page Root page number
     * @param ctx Error context
     * @return Unique pointer to opened index, or nullptr on error
     */
    static std::unique_ptr<FullTextIndex> open(Database* db,
                                               const ID& index_uuid,
                                               const ID& table_uuid,
                                               const std::vector<ID>& column_ids,
                                               uint32_t root_page,
                                               ErrorContext* ctx = nullptr);

    /**
     * @brief Destructor
     */
    ~FullTextIndex();

    // Disable copy/move
    FullTextIndex(const FullTextIndex&) = delete;
    FullTextIndex& operator=(const FullTextIndex&) = delete;

    /**
     * @brief Insert a tsvector value into the index
     *
     * Extracts lexemes from the tsvector and indexes them. Each unique lexeme
     * becomes a key in the GIN index, with the TID added to its posting list.
     *
     * Firebird MGA: The TID must be stable across transaction versions unless
     * the indexed column changes.
     *
     * @param tsvector_data Serialized tsvector data
     * @param tsvector_len Length of serialized data
     * @param tid Tuple identifier
     * @param ctx Error context
     * @return Status code
     */
    Status insert(const void* tsvector_data, size_t tsvector_len,
                  const TID& tid, ErrorContext* ctx = nullptr);

    /**
     * @brief Search for documents matching a tsquery
     *
     * Evaluates the Boolean expression in tsquery against indexed tsvector
     * values and returns TIDs of matching documents.
     *
     * Firebird MGA: Uses TIP-based visibility filtering with current_xid
     * (NOT PostgreSQL snapshots).
     *
     * @param tsquery The search query
     * @param current_xid Current transaction ID for visibility checks
     * @param ctx Error context
     * @return Vector of matching TIDs
     */
    std::vector<TID> search(const TSQuery& tsquery,
                           uint64_t current_xid,
                           ErrorContext* ctx = nullptr);

    /**
     * @brief Get index statistics
     *
     * @return Statistics from the underlying GIN index
     */
    GinIndex::Statistics getStatistics();

    /**
     * @brief Get the underlying GIN index (for advanced operations)
     *
     * @return Pointer to the GIN index (owned by this object)
     */
    GinIndex* getGinIndex() { return gin_index_.get(); }

private:
    Database* db_;
    ID index_uuid_;
    ID table_uuid_;
    std::vector<ID> column_ids_;
    std::unique_ptr<GinIndex> gin_index_;
};

} // namespace scratchbird::core
