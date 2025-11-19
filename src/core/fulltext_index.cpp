#include "scratchbird/core/fulltext_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"

namespace scratchbird::core
{

FullTextIndex::FullTextIndex(Database* db,
                             const ID& index_uuid,
                             const ID& table_uuid,
                             const std::vector<ID>& column_ids,
                             std::unique_ptr<GinIndex> gin_index)
    : db_(db)
    , index_uuid_(index_uuid)
    , table_uuid_(table_uuid)
    , column_ids_(column_ids)
    , gin_index_(std::move(gin_index))
{
}

FullTextIndex::~FullTextIndex() = default;

Status FullTextIndex::create(Database* db,
                             const ID& index_uuid,
                             const ID& table_uuid,
                             const std::vector<ID>& column_ids,
                             uint32_t* root_page_out,
                             ErrorContext* ctx)
{
    if (!db || !root_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Invalid arguments to FullTextIndex::create");
        return Status::INVALID_ARGUMENT;
    }

    if (column_ids.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "FullTextIndex requires at least one column");
        return Status::INVALID_ARGUMENT;
    }

    // Create underlying GIN index
    Status status = GinIndex::create(db, index_uuid.bytes, root_page_out, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    return Status::OK;
}

std::unique_ptr<FullTextIndex> FullTextIndex::open(Database* db,
                                                   const ID& index_uuid,
                                                   const ID& table_uuid,
                                                   const std::vector<ID>& column_ids,
                                                   uint32_t root_page,
                                                   ErrorContext* ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Invalid arguments to FullTextIndex::open");
        return nullptr;
    }

    // Open underlying GIN index
    auto gin_index = GinIndex::open(db, index_uuid.bytes, root_page, ctx);
    if (!gin_index)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
            "Failed to open underlying GIN index");
        return nullptr;
    }

    // Create FullTextIndex wrapper
    return std::unique_ptr<FullTextIndex>(
        new FullTextIndex(db, index_uuid, table_uuid, column_ids,
                         std::move(gin_index)));
}

Status FullTextIndex::insert(const void* tsvector_data, size_t tsvector_len,
                             const TID& tid, ErrorContext* ctx)
{
    if (!tsvector_data || tsvector_len == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Invalid tsvector data");
        return Status::INVALID_ARGUMENT;
    }

    // Use GINTSVectorOps to extract keys from the tsvector
    auto key_extractor = makeGINTSVectorKeyExtractor();

    // Insert into GIN index
    return gin_index_->insert(tsvector_data, tsvector_len, tid,
                             key_extractor, ctx);
}

std::vector<TID> FullTextIndex::search(const TSQuery& tsquery,
                                       uint64_t current_xid,
                                       ErrorContext* ctx)
{
    // Extract search keys from the query
    auto query_keys = GINTSVectorOps::extractQueryKeys(tsquery);

    if (query_keys.empty())
    {
        // Empty query matches nothing
        return {};
    }

    // Analyze query structure to determine lookup strategy
    auto strategy = GINTSVectorOps::analyzeQuery(tsquery);

    std::vector<TID> candidates;

    switch (strategy)
    {
        case GINTSVectorOps::QueryStrategy::NEED_ALL:
            // AND query: documents must contain all keys
            candidates = gin_index_->findAll(query_keys, current_xid, ctx);
            break;

        case GINTSVectorOps::QueryStrategy::NEED_ANY:
            // OR query: documents must contain any key
            candidates = gin_index_->findAny(query_keys, current_xid, ctx);
            break;

        case GINTSVectorOps::QueryStrategy::NEED_RECHECK:
        default:
            // Complex query: get candidates containing any key, then recheck
            candidates = gin_index_->findAny(query_keys, current_xid, ctx);
            break;
    }

    // For complex queries or when we need to verify exact matches,
    // we would recheck each candidate against the full tsquery.
    // This requires fetching the tsvector value for each TID.
    //
    // For now, we return the candidates. Full recheck would require
    // access to the table data, which would be implemented in the
    // executor layer.

    return candidates;
}

GinIndex::Statistics FullTextIndex::getStatistics()
{
    return gin_index_->getStatistics();
}

} // namespace scratchbird::core
