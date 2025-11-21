# Index Factory Integration - Function Signature Analysis

## Simple Integration (UUID + meta_page only):
- **HASH**: create(db, uuid, *meta_page_out, ctx) / open(db, uuid, meta_page, ctx) ✅
- **GIN**: create(db, uuid, *meta_page_out, ctx) / open(db, uuid, meta_page, ctx) ✅
- **BITMAP**: create(db, uuid, *meta_page_out, ctx) / open(db, uuid, meta_page, ctx) ✅

## Complex Integration (requires additional parameters):
- **HNSW**: Needs dimensions, distance_metric, m, ef_construction, ef_search
- **BRIN**: Needs value_type (DataType), range_size
- **RTREE**: Needs max_entries parameter
- **COLUMNSTORE**: Needs column_uuids vector, page_size, compression_type

## Solution:
1. Integrate HASH, GIN, BITMAP fully (simple signatures)
2. Mark HNSW, BRIN, RTREE, COLUMNSTORE as NOT_IMPLEMENTED with message about needing configuration
3. Add TODO comments for future proper integration with index-specific parameters
