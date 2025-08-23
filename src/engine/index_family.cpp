#include "scratchbird/engine/index_family.h"

#include "scratchbird/engine/index_bitmap.h"
#include "scratchbird/engine/index_btree.h"
#include "scratchbird/engine/index_gin.h"
#include "scratchbird/engine/index_hash.h"
#include "scratchbird/engine/index_rtree.h"

#include <stdexcept>

namespace scratchbird::engine
{

    std::unique_ptr<IndexFamily> IndexFamilyFactory::create_index(IndexMethod method, FileMap fmap,
                                                                  std::uint32_t page_size,
                                                                  bool unique)
    {
        switch (method) {
        case IndexMethod::BTree:
            return std::make_unique<BTreeIndexFamily>(std::move(fmap), page_size, unique);

        case IndexMethod::Hash:
            return std::make_unique<HashIndex>(std::move(fmap), page_size, unique);

        case IndexMethod::Bitmap:
            return std::make_unique<BitmapIndex>(std::move(fmap), page_size, unique);

        case IndexMethod::Gin:
            return std::make_unique<GinIndex>(std::move(fmap), page_size, unique);

        case IndexMethod::RTree:
            return std::make_unique<RTreeIndex>(std::move(fmap), page_size, unique);

        case IndexMethod::PartialHash:
            // Specialized hash index variant
            return nullptr;

        default:
            throw std::invalid_argument("Unsupported index method");
        }
    }

    std::vector<ValidationMessage>
    IndexFamilyFactory::validate_method_options(const IndexCreateOptions& opts)
    {
        std::vector<ValidationMessage> messages;

        switch (opts.method) {
        case IndexMethod::BTree:
            // B-Tree supports all options
            break;

        case IndexMethod::Hash:
            if (!opts.where_predicate.empty()) {
                messages.push_back({false, "Hash indexes support partial indexes"});
            }
            if (opts.keys.size() != 1) {
                messages.push_back({true, "Hash indexes support only single-column keys"});
            }
            break;

        case IndexMethod::Bitmap:
            if (opts.unique) {
                messages.push_back({true, "Bitmap indexes cannot be unique"});
            }
            if (opts.keys.size() != 1) {
                messages.push_back({true, "Bitmap indexes support only single-column keys"});
            }
            break;

        case IndexMethod::Gin:
            if (opts.unique) {
                messages.push_back({true, "GIN indexes cannot be unique"});
            }
            break;

        case IndexMethod::RTree:
            if (opts.unique) {
                messages.push_back({true, "R-Tree indexes cannot be unique"});
            }
            if (opts.keys.size() != 1) {
                messages.push_back({true, "R-Tree indexes support only single-column keys"});
            }
            if (!opts.include_columns.empty()) {
                messages.push_back({false, "R-Tree indexes do not benefit from INCLUDE columns"});
            }
            break;

        case IndexMethod::PartialHash:
            if (opts.where_predicate.empty()) {
                messages.push_back({true, "Partial hash indexes require WHERE clause"});
            }
            break;

        default:
            messages.push_back({true, "Unknown index method"});
            break;
        }

        return messages;
    }

    bool IndexFamilyFactory::supports_range_queries(IndexMethod method)
    {
        switch (method) {
        case IndexMethod::BTree:
        case IndexMethod::RTree:
            return true;
        case IndexMethod::Hash:
        case IndexMethod::Bitmap:
        case IndexMethod::Gin:
        case IndexMethod::PartialHash:
            return false;
        }
        return false;
    }

    bool IndexFamilyFactory::supports_partial_indexes(IndexMethod method)
    {
        switch (method) {
        case IndexMethod::BTree:
        case IndexMethod::Hash:
        case IndexMethod::PartialHash:
            return true;
        case IndexMethod::Bitmap:
        case IndexMethod::Gin:
        case IndexMethod::RTree:
            return false; // These methods have their own filtering mechanisms
        }
        return false;
    }

    bool IndexFamilyFactory::supports_include_columns(IndexMethod method)
    {
        switch (method) {
        case IndexMethod::BTree:
            return true;
        case IndexMethod::Hash:
            return true; // Hash indexes can store payload
        case IndexMethod::Bitmap:
        case IndexMethod::Gin:
        case IndexMethod::RTree:
        case IndexMethod::PartialHash:
            return false;
        }
        return false;
    }

    bool IndexFamilyFactory::supports_expression_indexes(IndexMethod method)
    {
        switch (method) {
        case IndexMethod::BTree:
        case IndexMethod::Hash:
        case IndexMethod::PartialHash:
            return true;
        case IndexMethod::Bitmap:
        case IndexMethod::Gin:
        case IndexMethod::RTree:
            return false; // Require column values directly
        }
        return false;
    }

    // Hash Index Scan Implementation
    bool HashIndexScan::init(const std::string& key_condition)
    {
        reset();
        current_key_ = key_condition;

        // For hash index, we only support exact match
        if (index_) {
            index_->search_equal(current_key_, results_);
            pages_accessed_++;
        }

        return !results_.empty();
    }

    bool HashIndexScan::next(std::uint64_t& row_id, std::string& key, std::string& payload)
    {
        if (result_index_ >= results_.size()) {
            finished_ = true;
            return false;
        }

        row_id = results_[result_index_++];
        key = current_key_;
        payload.clear(); // Hash scan doesn't return payload by default
        rows_scanned_++;

        return true;
    }

    void HashIndexScan::reset()
    {
        finished_ = false;
        rows_scanned_ = 0;
        pages_accessed_ = 0;
        result_index_ = 0;
        results_.clear();
    }

    bool HashIndexScan::is_finished() const
    {
        return finished_;
    }

    std::uint64_t HashIndexScan::rows_scanned() const
    {
        return rows_scanned_;
    }

    std::uint64_t HashIndexScan::pages_accessed() const
    {
        return pages_accessed_;
    }

    // Bitmap Index Scan Implementation
    bool BitmapIndexScan::init(const std::string& key_condition)
    {
        reset();

        // Bitmap index scan involves loading bitmap for the condition
        // This is a placeholder implementation
        if (index_) {
            // Load bitmap data for the key condition
            pages_accessed_++;
        }

        return !bitmap_.empty();
    }

    bool BitmapIndexScan::next(std::uint64_t& row_id, std::string& key, std::string& payload)
    {
        // Scan through bitmap to find next set bit
        while (bitmap_position_ < bitmap_.size() * 8) {
            std::size_t byte_idx = bitmap_position_ / 8;
            std::size_t bit_idx = bitmap_position_ % 8;

            if (bitmap_[byte_idx] & (1 << bit_idx)) {
                row_id = bitmap_position_;
                key.clear(); // Bitmap scan doesn't track individual keys
                payload.clear();
                rows_scanned_++;
                bitmap_position_++;
                return true;
            }
            bitmap_position_++;
        }

        finished_ = true;
        return false;
    }

    void BitmapIndexScan::reset()
    {
        finished_ = false;
        rows_scanned_ = 0;
        pages_accessed_ = 0;
        bitmap_position_ = 0;
        bitmap_.clear();
    }

    bool BitmapIndexScan::is_finished() const
    {
        return finished_;
    }

    std::uint64_t BitmapIndexScan::rows_scanned() const
    {
        return rows_scanned_;
    }

    std::uint64_t BitmapIndexScan::pages_accessed() const
    {
        return pages_accessed_;
    }

    // GIN Index Scan Implementation
    bool GinIndexScan::init(const std::string& key_condition)
    {
        reset();

        // Tokenize the search condition
        // This is a simplified tokenizer - real implementation would be more sophisticated
        std::string token;
        for (char c : key_condition) {
            if (c == ' ' || c == '\t' || c == '\n') {
                if (!token.empty()) {
                    tokens_.push_back(token);
                    token.clear();
                }
            } else {
                token += c;
            }
        }
        if (!token.empty()) {
            tokens_.push_back(token);
        }

        if (index_ && !tokens_.empty()) {
            pages_accessed_++;
        }

        return !tokens_.empty();
    }

    bool GinIndexScan::next(std::uint64_t& row_id, std::string& key, std::string& payload)
    {
        // This is a simplified implementation
        // Real GIN would intersect posting lists for all tokens
        while (token_index_ < tokens_.size()) {
            if (posting_index_ < posting_list_.size()) {
                row_id = posting_list_[posting_index_++];
                key = tokens_[token_index_];
                payload.clear();
                rows_scanned_++;
                return true;
            }

            // Move to next token
            token_index_++;
            posting_index_ = 0;
            posting_list_.clear();

            // Load posting list for current token (placeholder)
            if (token_index_ < tokens_.size()) {
                // Would load actual posting list here
                pages_accessed_++;
            }
        }

        finished_ = true;
        return false;
    }

    void GinIndexScan::reset()
    {
        finished_ = false;
        rows_scanned_ = 0;
        pages_accessed_ = 0;
        token_index_ = 0;
        posting_index_ = 0;
        tokens_.clear();
        posting_list_.clear();
    }

    bool GinIndexScan::is_finished() const
    {
        return finished_;
    }

    std::uint64_t GinIndexScan::rows_scanned() const
    {
        return rows_scanned_;
    }

    std::uint64_t GinIndexScan::pages_accessed() const
    {
        return pages_accessed_;
    }

    // R-Tree Index Scan Implementation
    bool RTreeIndexScan::init(const std::string& key_condition)
    {
        reset();

        // Parse spatial query condition (simplified)
        // Real implementation would parse WKT or other spatial formats
        query_rect_ = {0.0, 0.0, 100.0, 100.0}; // Default query rectangle

        if (index_) {
            // Perform spatial search (placeholder)
            pages_accessed_++;
        }

        return !results_.empty();
    }

    bool RTreeIndexScan::next(std::uint64_t& row_id, std::string& key, std::string& payload)
    {
        if (result_index_ >= results_.size()) {
            finished_ = true;
            return false;
        }

        auto& result = results_[result_index_++];
        row_id = result.first;

        // Encode rectangle as key (simplified)
        auto& rect = result.second;
        key = std::to_string(rect.min_x) + "," + std::to_string(rect.min_y) + "," +
              std::to_string(rect.max_x) + "," + std::to_string(rect.max_y);
        payload.clear();
        rows_scanned_++;

        return true;
    }

    void RTreeIndexScan::reset()
    {
        finished_ = false;
        rows_scanned_ = 0;
        pages_accessed_ = 0;
        result_index_ = 0;
        results_.clear();
        query_rect_ = {0.0, 0.0, 0.0, 0.0};
    }

    bool RTreeIndexScan::is_finished() const
    {
        return finished_;
    }

    std::uint64_t RTreeIndexScan::rows_scanned() const
    {
        return rows_scanned_;
    }

    std::uint64_t RTreeIndexScan::pages_accessed() const
    {
        return pages_accessed_;
    }

} // namespace scratchbird::engine
