#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/compression.h"
#include <cstring>
#include <algorithm>

// Simple value structure for TOAST usage
namespace scratchbird {
namespace core {

struct Value {
    enum Type { INT32, BYTEA };
    Type type;
    union {
        int32_t int_val;
        struct {
            const uint8_t* data;
            uint32_t size;
        } bytea_val;
    };
    
    Value() : type(INT32), int_val(0) {}
    Value(int32_t val) : type(INT32), int_val(val) {}
    Value(const std::vector<uint8_t>& data) : type(BYTEA) {
        bytea_val.data = data.data();
        bytea_val.size = data.size();
    }
    Value(std::vector<uint8_t>&& data) : type(BYTEA) {
        bytea_val.data = data.data();
        bytea_val.size = data.size();
    }
};

} // namespace core
} // namespace scratchbird

namespace scratchbird {
namespace core {

ToastManager::ToastManager(Database* db, const ID& table_id)
    : db_(db), table_id_(table_id), toast_table_id_(), next_value_id_(1) {
}

ToastManager::~ToastManager() = default;

Status ToastManager::initialize(ErrorContext* ctx) {
    // Check if TOAST table already exists
    CatalogManager* catalog = db_->catalog_manager();
    
    // TOAST table naming convention: pg_toast_<table_id>
    std::string toast_name = "pg_toast_" + table_id_.to_string();
    
    // Get the schema of the parent table first
    CatalogManager::TableInfo parent_info;
    Status status = catalog->get_table(table_id_, parent_info, ctx);
    if (status != Status::Ok) {
        // If we can't get parent table info, something is wrong
        SET_ERROR_CONTEXT(ctx, status, "Failed to get parent table info");
        return status;
    }
    
    CatalogManager::TableInfo info;
    status = catalog->get_table(parent_info.schema_id, toast_name, info, ctx);
    if (status == Status::Ok) {
        // TOAST table already exists
        toast_table_id_ = info.table_id;
        
        // TODO: Read max value_id from TOAST table to set next_value_id_
        // For now, start from a high number to avoid conflicts
        next_value_id_ = 1000000;
        
        return Status::Ok;
    }
    
    // Create TOAST table if it doesn't exist
    return create_toast_table(ctx);
}

Status ToastManager::create_toast_table(ErrorContext* ctx) {
    CatalogManager* catalog = db_->catalog_manager();
    
    // TOAST table schema:
    // chunk_id: INT (TOAST value ID)
    // chunk_seq: INT (sequence number)
    // chunk_data: BYTEA (actual data)
    std::vector<CatalogManager::ColumnInfo> columns;
    
    // chunk_id column
    CatalogManager::ColumnInfo col1;
    col1.column_id = 0;
    col1.column_name = "chunk_id";
    col1.data_type = static_cast<uint16_t>(DataType::Int);
    col1.max_length = 4;
    col1.nullable = false;
    col1.has_default = false;
    columns.push_back(col1);
    
    // chunk_seq column
    CatalogManager::ColumnInfo col2;
    col2.table_id = 0;
    col2.column_id = 1;
    col2.column_name = "chunk_seq";
    col2.data_type = static_cast<uint16_t>(DataType::Int);
    col2.max_length = 4;
    col2.nullable = false;
    col2.has_default = false;
    columns.push_back(col2);
    
    // chunk_data column
    CatalogManager::ColumnInfo col3;
    col3.table_id = 0;
    col3.column_id = 2;
    col3.column_name = "chunk_data";
    col3.data_type = static_cast<uint16_t>(DataType::Bytea);
    col3.max_length = TOAST_MAX_CHUNK_SIZE;
    col3.nullable = false;
    col3.has_default = false;
    columns.push_back(col3);
    
    std::string toast_name = "pg_toast_" + table_id_.to_string();
    
    // Get the schema of the parent table
    CatalogManager::TableInfo parent_info;
    Status status = catalog->get_table(table_id_, parent_info, ctx);
    if (status != Status::Ok) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to get parent table info");
        return status;
    }
    
    status = catalog->create_table(parent_info.schema_id, toast_name, columns, 
                                 toast_table_id_, ctx);
    if (status != Status::Ok) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to create TOAST table");
        return status;
    }
    
    // Create index on (chunk_id, chunk_seq) for efficient retrieval
    std::vector<std::string> index_columns = {"chunk_id", "chunk_seq"};
    ID index_id;
    std::string index_name = toast_name + "_idx";
    status = catalog->create_index(toast_table_id_, index_name, index_columns, index_id, false, ctx);
    if (status != Status::Ok) {
        // This is not fatal, but we should log it
        // TODO: Add logging
    }
    
    return Status::Ok;
}

Status ToastManager::toast_value(const uint8_t* data, uint32_t size,
                               ToastStrategy strategy, uint64_t xmin,
                               ToastPointer* pointer_out,
                               ErrorContext* ctx) {
    if (!data || !pointer_out) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Null pointer in toast_value");
        return Status::InvalidArgument;
    }
    
    // Assign unique value ID
    uint32_t value_id = next_value_id_++;
    
    // Initialize pointer
    pointer_out->va_header = 0x01;  // TOAST marker
    pointer_out->va_tag = static_cast<uint8_t>(strategy);
    pointer_out->va_rawsize = size;
    pointer_out->va_valueid = value_id;
    pointer_out->va_toastrelid = 0; //toast_table_id_;
    
    // Handle based on strategy
    switch (strategy) {
        case ToastStrategy::PLAIN:
            // Shouldn't happen - PLAIN means no TOAST
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                             "PLAIN strategy used for TOAST");
            return Status::InvalidArgument;
            
        case ToastStrategy::EXTENDED:
            // Store out-of-line, uncompressed
            pointer_out->va_extsize = size;
            return write_toast_chunks(value_id, data, size, xmin, ctx);
            
        case ToastStrategy::COMPRESSED:
            // This strategy would store compressed inline - not for TOAST
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                             "COMPRESSED strategy not supported for TOAST");
            return Status::InvalidArgument;
            
        case ToastStrategy::EXTERNAL: {
            // Store out-of-line, compressed
            std::vector<uint8_t> compressed;
            Status status = compress_data(data, size, &compressed, ctx);
            if (status != Status::Ok) {
                // Fall back to uncompressed
                pointer_out->va_tag = static_cast<uint8_t>(ToastStrategy::EXTENDED);
                pointer_out->va_extsize = size;
                return write_toast_chunks(value_id, data, size, xmin, ctx);
            }
            
            pointer_out->va_extsize = compressed.size();
            return write_toast_chunks(value_id, compressed.data(), 
                                    compressed.size(), xmin, ctx);
        }
        
        default:
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                             "Unknown TOAST strategy");
            return Status::InvalidArgument;
    }
}

Status ToastManager::detoast_value(const ToastPointer* pointer,
                                 std::vector<uint8_t>* data_out,
                                 uint64_t xmin,
                                 ErrorContext* ctx) {
    if (!pointer || !data_out) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Null pointer in detoast_value");
        return Status::InvalidArgument;
    }
    
    // Verify it's a TOAST pointer
    if (pointer->va_header != 0x01) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Not a TOAST pointer");
        return Status::InvalidArgument;
    }
    
    ToastStrategy strategy = static_cast<ToastStrategy>(pointer->va_tag);
    
    switch (strategy) {
        case ToastStrategy::EXTENDED: {
            // Read uncompressed chunks
            data_out->clear();
            return read_toast_chunks(pointer->va_valueid, data_out, xmin, ctx);
        }
        
        case ToastStrategy::EXTERNAL: {
            // Read compressed chunks and decompress
            std::vector<uint8_t> compressed;
            Status status = read_toast_chunks(pointer->va_valueid, 
                                            &compressed, xmin, ctx);
            if (status != Status::Ok) {
                return status;
            }
            
            return decompress_data(compressed.data(), compressed.size(),
                                 pointer->va_rawsize, data_out, ctx);
        }
        
        default:
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                             "Invalid TOAST strategy in pointer");
            return Status::InvalidArgument;
    }
}

Status ToastManager::delete_toast_value(uint32_t value_id, uint64_t xmax,
                                      ErrorContext* ctx) {
    // StorageEngine* storage = db_->storage_engine();
    // CatalogManager* catalog = db_->catalog_manager();

    // // Get the index ID for the TOAST table
    // std::string toast_name = "pg_toast_" + table_id_.to_string();
    // std::string index_name = toast_name + "_idx";
    // CatalogManager::IndexInfo index_info;
    // Status status = catalog->get_index(toast_table_id_, index_name, index_info, ctx);
    // if (status != Status::Ok) {
    //     // Fall back to heap scan if index not found
    //     return delete_toast_value_heap_scan(value_id, xmax, ctx);
    // }

    // // Create an index scan
    // auto scan = storage->create_index_scan(index_info.index_id, ctx);
    // if (!scan) {
    //     SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
    //                      "Failed to create index scan for TOAST table");
    //     return Status::InvalidArgument;
    // }

    // // Seek to the first chunk for this value_id
    // std::vector<uint8_t> key;
    // key.insert(key.end(), reinterpret_cast<uint8_t*>(&value_id), reinterpret_cast<uint8_t*>(&value_id) + 4);
    // status = scan->seek(key, ctx);
    // if (status != Status::Ok) {
    //     return status;
    // }

    // // ... rest of implementation using index scan ...

    return delete_toast_value_heap_scan(value_id, xmax, ctx);
}

Status ToastManager::delete_toast_value_heap_scan(uint32_t value_id, uint64_t xmax,
                                                  ErrorContext* ctx) {
    StorageEngine* storage = db_->storage_engine();
    
    // Mark all chunks of this value as deleted
    // In a real implementation, this would use an index on chunk_id
    // For now, we'll scan the TOAST table (inefficient but correct)
    
    auto scan = storage->create_scan(toast_table_id_, ctx);
    if (!scan) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Failed to scan TOAST table");
        return Status::InvalidArgument;
    }
    
    Tuple tuple;
    Status status;
    while ((status = scan->next(&tuple, ctx)) == Status::Ok) {
        if (!tuple.data || tuple.data_size < 4) continue;
        
        uint32_t chunk_id = *reinterpret_cast<const uint32_t*>(tuple.data);
        if (chunk_id == value_id) {
            // Delete this chunk
            Status delete_status = storage->delete_tuple(tuple.page_id, tuple.item_id, ctx);
            if (delete_status != Status::Ok) {
                return delete_status;
            }
        }
    }
    
    return Status::Ok;
}

ToastStrategy ToastManager::choose_strategy(const uint8_t* data, uint32_t size,
                                          bool compress_enabled) {
    // Simple strategy selection
    if (size <= TOAST_TUPLE_THRESHOLD) {
        return ToastStrategy::PLAIN;  // Don't TOAST small values
    }
    
    if (!compress_enabled) {
        return ToastStrategy::EXTENDED;  // Out-of-line, uncompressed
    }
    
    // For large values, try compression
    // In a real implementation, we might sample the data to estimate
    // compressibility before deciding
    if (size > TOAST_TUPLE_THRESHOLD * 2) {
        return ToastStrategy::EXTERNAL;  // Out-of-line, compressed
    }
    
    return ToastStrategy::EXTENDED;  // Out-of-line, uncompressed
}

Status ToastManager::write_toast_chunks(uint32_t value_id, const uint8_t* data,
                                      uint32_t size, uint64_t xmin,
                                      ErrorContext* ctx) {
    StorageEngine* storage = db_->storage_engine();
    
    // Split data into chunks
    uint32_t chunks_needed = (size + TOAST_MAX_CHUNK_SIZE - 1) / TOAST_MAX_CHUNK_SIZE;
    uint32_t offset = 0;
    
    for (uint32_t seq = 0; seq < chunks_needed; seq++) {
        uint32_t chunk_size = std::min(TOAST_MAX_CHUNK_SIZE, size - offset);
        
        // Build tuple data manually
        // Format: chunk_id (4 bytes) | chunk_seq (4 bytes) | chunk_size (4 bytes) | data
        std::vector<uint8_t> tuple_data;
        tuple_data.reserve(12 + chunk_size);
        
        // Add chunk_id
        uint32_t id = value_id;
        tuple_data.insert(tuple_data.end(), 
                         reinterpret_cast<uint8_t*>(&id),
                         reinterpret_cast<uint8_t*>(&id) + 4);
        
        // Add chunk_seq
        tuple_data.insert(tuple_data.end(),
                         reinterpret_cast<uint8_t*>(&seq),
                         reinterpret_cast<uint8_t*>(&seq) + 4);
        
        // Add chunk_size
        tuple_data.insert(tuple_data.end(),
                         reinterpret_cast<uint8_t*>(&chunk_size),
                         reinterpret_cast<uint8_t*>(&chunk_size) + 4);
        
        // Add chunk data
        tuple_data.insert(tuple_data.end(),
                         data + offset,
                         data + offset + chunk_size);
        
        // Insert tuple
        uint32_t page_id;
        uint16_t item_id;
        Status status = storage->insert_tuple(toast_table_id_, tuple_data.data(),
                                            tuple_data.size(), &page_id, 
                                            &item_id, ctx);
        if (status != Status::Ok) {
            SET_ERROR_CONTEXT(ctx, status, "Failed to insert TOAST chunk");
            // TODO: Clean up any chunks we already inserted
            return status;
        }
        
        offset += chunk_size;
    }
    
    return Status::Ok;
}

Status ToastManager::read_toast_chunks(uint32_t value_id, 
                                     std::vector<uint8_t>* data_out,
                                     uint64_t xmin, ErrorContext* ctx) {
    // StorageEngine* storage = db_->storage_engine();
    // CatalogManager* catalog = db_->catalog_manager();

    // // Get the index ID for the TOAST table
    // std::string toast_name = "pg_toast_" + table_id_.to_string();
    // std::string index_name = toast_name + "_idx";
    // CatalogManager::IndexInfo index_info;
    // Status status = catalog->get_index(toast_table_id_, index_name, index_info, ctx);
    // if (status != Status::Ok) {
    //     // Fall back to heap scan if index not found
    //     return read_toast_chunks_heap_scan(value_id, data_out, xmin, ctx);
    // }

    // // Create an index scan
    // auto scan = storage->create_index_scan(index_info.index_id, ctx);
    // if (!scan) {
    //     SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
    //                      "Failed to create index scan for TOAST table");
    //     return Status::InvalidArgument;
    // }

    // // Seek to the first chunk for this value_id
    // std::vector<uint8_t> key;
    // key.insert(key.end(), reinterpret_cast<uint8_t*>(&value_id), reinterpret_cast<uint8_t*>(&value_id) + 4);
    // status = scan->seek(key, ctx);
    // if (status != Status::Ok) {
    //     return status;
    // }
    
    // // ... rest of implementation using index scan ...

    return read_toast_chunks_heap_scan(value_id, data_out, xmin, ctx);
}

Status ToastManager::read_toast_chunks_heap_scan(uint32_t value_id,
                                     std::vector<uint8_t>* data_out,
                                     uint64_t xmin, ErrorContext* ctx) {
    StorageEngine* storage = db_->storage_engine();
    
    // Scan TOAST table for chunks with this value_id
    // In a real implementation, we'd use an index
    auto scan = storage->create_scan(toast_table_id_, ctx);
    if (!scan) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Failed to scan TOAST table");
        return Status::InvalidArgument;
    }
    
    // Collect chunks
    struct ChunkData {
        uint32_t seq;
        std::vector<uint8_t> data;
    };
    std::vector<ChunkData> chunks;
    
    Tuple tuple;
    Status status;
    while ((status = scan->next(&tuple, ctx)) == Status::Ok) {
        // Parse tuple format: chunk_id | chunk_seq | chunk_size | data
        if (!tuple.data || tuple.data_size < 12) continue;
        
        const uint8_t* ptr = tuple.data;
        uint32_t chunk_id = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;
        
        if (chunk_id != value_id) continue;
        
        uint32_t chunk_seq = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;
        
        uint32_t chunk_size = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;
        
        // Validate chunk size
        if (chunk_size > TOAST_MAX_CHUNK_SIZE || 
            12 + chunk_size > tuple.data_size) {
            continue;
        }
        
        // Extract chunk data
        std::vector<uint8_t> chunk_data(chunk_size);
        memcpy(chunk_data.data(), ptr, chunk_size);
        
        chunks.push_back({chunk_seq, std::move(chunk_data)});
    }
    
    if (chunks.empty()) {
        SET_ERROR_CONTEXT(ctx, Status::NotFound, 
                         "TOAST value not found");
        return Status::NotFound;
    }
    
    // Sort chunks by sequence number
    std::sort(chunks.begin(), chunks.end(), 
              [](const ChunkData& a, const ChunkData& b) {
                  return a.seq < b.seq;
              });
    
    // Reassemble data
    data_out->clear();
    for (const auto& chunk : chunks) {
        data_out->insert(data_out->end(), 
                        chunk.data.begin(), chunk.data.end());
    }
    
    return Status::Ok;
}

Status ToastManager::compress_data(const uint8_t* src, uint32_t src_size,
                                 std::vector<uint8_t>* dst,
                                 ErrorContext* ctx) {
    // Use compression framework if available
    auto codec = CompressionFactory::create(CompressionType::LZ4);
    if (!codec) {
        // No compression available
        return Status::InvalidArgument;
    }
    
    // Reserve space for compressed header + data
    uint32_t max_size = codec->max_compressed_size(src_size);
    dst->resize(sizeof(ToastCompressHeader) + max_size);
    
    // Write header
    ToastCompressHeader* header = reinterpret_cast<ToastCompressHeader*>(dst->data());
    header->rawsize = src_size;
    header->compression = static_cast<uint8_t>(CompressionType::LZ4);
    
    // Compress data
    uint32_t compressed_size;
    Status status = codec->compress(src, src_size,
                                  dst->data() + sizeof(ToastCompressHeader),
                                  max_size, &compressed_size);
    if (status != Status::Ok) {
        return status;
    }
    
    // Resize to actual size
    dst->resize(sizeof(ToastCompressHeader) + compressed_size);
    
    // Only use compression if it saves space
    if (dst->size() >= src_size * 0.9) {
        return Status::InvalidArgument;  // Not worth compressing
    }
    
    return Status::Ok;
}

Status ToastManager::decompress_data(const uint8_t* src, uint32_t src_size,
                                   uint32_t uncompressed_size,
                                   std::vector<uint8_t>* dst,
                                   ErrorContext* ctx) {
    if (src_size < sizeof(ToastCompressHeader)) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Invalid compressed TOAST data");
        return Status::InvalidArgument;
    }
    
    const ToastCompressHeader* header = 
        reinterpret_cast<const ToastCompressHeader*>(src);
    
    if (header->rawsize != uncompressed_size) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Compressed size mismatch");
        return Status::InvalidArgument;
    }
    
    CompressionType comp_type = static_cast<CompressionType>(header->compression);
    auto codec = CompressionFactory::create(comp_type);
    if (!codec) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Compression type not supported");
        return Status::InvalidArgument;
    }
    
    dst->resize(uncompressed_size);
    uint32_t decompressed_size;
    
    Status status = codec->decompress(
        src + sizeof(ToastCompressHeader),
        src_size - sizeof(ToastCompressHeader),
        dst->data(),
        uncompressed_size,
        &decompressed_size
    );
    
    if (status != Status::Ok || decompressed_size != uncompressed_size) {
        SET_ERROR_CONTEXT(ctx, Status::CompressionError, 
                         "Failed to decompress TOAST data");
        return Status::CompressionError;
    }
    
    return Status::Ok;
}

} // namespace core
} // namespace scratchbirde scratchbird