#include "scratchbird/engine/index_gin.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>

namespace scratchbird::engine
{

    // SimpleTokenizer implementation
    std::vector<std::string> SimpleTokenizer::tokenize(const std::string& text)
    {
        std::vector<std::string> tokens;
        std::string current_token;

        for (char c : text) {
            if (std::isspace(c) || std::ispunct(c)) {
                if (!current_token.empty() && current_token.length() <= max_token_length_) {
                    if (!case_sensitive_) {
                        std::transform(current_token.begin(), current_token.end(),
                                       current_token.begin(), ::tolower);
                    }
                    tokens.push_back(current_token);
                    current_token.clear();
                }
            } else if (std::isalnum(c)) {
                current_token += c;
            }
        }

        // Add last token
        if (!current_token.empty() && current_token.length() <= max_token_length_) {
            if (!case_sensitive_) {
                std::transform(current_token.begin(), current_token.end(), current_token.begin(),
                               ::tolower);
            }
            tokens.push_back(current_token);
        }

        return tokens;
    }

    // GinIndex implementation
    GinIndex::GinIndex(FileMap fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique)
    {
        // Initialize default tokenizer
        tokenizer_ =
            std::make_unique<SimpleTokenizer>(tunables_.case_sensitive, tunables_.max_token_length);

        // Initialize common stop words
        stop_words_ = {"a",   "an",   "and",  "are", "as", "at",  "be",   "by",
                       "for", "from", "has",  "he",  "in", "is",  "it",   "its",
                       "of",  "on",   "that", "the", "to", "was", "will", "with"};
    }

    bool GinIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        try {
            auto tokens = tokenize_text(key);

            for (const auto& token : tokens) {
                if (!token_insert(token, row_id, err)) {
                    return false;
                }
            }

            stats_dirty_ = true;
            return true;
        } catch (const std::exception& e) {
            err = "GIN insert failed: " + std::string(e.what());
            return false;
        }
    }

    bool GinIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                       const std::string& payload, std::string& err)
    {
        // GIN indexes don't typically store payloads, but we can extend this
        (void)payload; // Suppress unused parameter warning
        return insert(key, row_id, err);
    }

    void GinIndex::search_equal(const std::string& key, std::vector<std::uint64_t>& out) const
    {
        // For exact match, tokenize and find intersection of all tokens
        auto tokens = tokenize_text(key);
        search_tokens(tokens, out);
    }

    void GinIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        // GIN indexes don't store payloads
        std::vector<std::uint64_t> row_ids;
        search_equal(key, row_ids);

        out.clear();
        out.reserve(row_ids.size());
        for (std::uint64_t row_id : row_ids) {
            out.emplace_back(row_id, "");
        }
    }

    void GinIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                bool hi_incl,
                                std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        // GIN indexes are not well-suited for traditional range queries
        (void)lo;
        (void)lo_incl;
        (void)hi;
        (void)hi_incl; // Suppress warnings
        out.clear();
    }

    std::size_t GinIndex::erase_equal(const std::string& key, std::string& err)
    {
        try {
            auto tokens = tokenize_text(key);
            std::size_t total_deleted = 0;

            // Find all row IDs that contain ALL tokens (exact match)
            std::vector<std::uint64_t> matching_rows;
            search_tokens(tokens, matching_rows);

            // Remove these row IDs from all token posting lists
            for (const auto& token : tokens) {
                auto it = posting_lists_.find(token);
                if (it != posting_lists_.end()) {
                    auto& row_ids = it->second.row_ids;
                    auto original_size = row_ids.size();

                    row_ids.erase(std::remove_if(row_ids.begin(), row_ids.end(),
                                                 [&matching_rows](std::uint64_t row_id) {
                                                     return std::find(matching_rows.begin(),
                                                                      matching_rows.end(),
                                                                      row_id) !=
                                                            matching_rows.end();
                                                 }),
                                  row_ids.end());

                    total_deleted += original_size - row_ids.size();
                    it->second.frequency = row_ids.size();
                }
            }

            stats_dirty_ = true;
            return total_deleted;
        } catch (const std::exception& e) {
            err = "GIN delete failed: " + std::string(e.what());
            return 0;
        }
    }

    bool GinIndex::validate(std::string& error) const
    {
        try {
            // Validate posting list consistency
            for (const auto& [token, entry] : posting_lists_) {
                if (entry.row_ids.empty()) {
                    error = "Empty posting list for token: " + token;
                    return false;
                }

                if (entry.frequency != entry.row_ids.size()) {
                    error = "Frequency mismatch for token: " + token;
                    return false;
                }

                // Check for duplicate row IDs
                auto sorted_ids = entry.row_ids;
                std::sort(sorted_ids.begin(), sorted_ids.end());
                auto unique_end = std::unique(sorted_ids.begin(), sorted_ids.end());
                if (unique_end != sorted_ids.end()) {
                    error = "Duplicate row IDs in posting list for token: " + token;
                    return false;
                }
            }
            return true;
        } catch (const std::exception& e) {
            error = "GIN validation error: " + std::string(e.what());
            return false;
        }
    }

    void GinIndex::rebuild_offline()
    {
        // Rebuild GIN index from scratch
        // This would involve scanning the base table and re-tokenizing all documents

        // Clear existing posting lists
        posting_lists_.clear();

        // Reset statistics
        meta_header_ = {};
        stats_dirty_ = true;

        // Compress all posting lists
        for (auto& [token, entry] : posting_lists_) {
            compress_posting_list(entry);
        }
    }

    std::string GinIndex::collect_statistics() const
    {
        if (stats_dirty_) {
            cached_stats_ = compute_statistics();
            stats_dirty_ = false;
        }

        std::ostringstream oss;
        oss << "GIN Index Statistics:\n"
            << "  Total Tokens: " << cached_stats_.total_tokens << "\n"
            << "  Total Documents: " << cached_stats_.total_documents << "\n"
            << "  Total Postings: " << cached_stats_.total_postings << "\n"
            << "  Compressed Postings: " << cached_stats_.compressed_postings << "\n"
            << "  Average Document Length: " << cached_stats_.average_document_length << "\n"
            << "  Compression Ratio: " << cached_stats_.compression_ratio << "\n";

        return oss.str();
    }

    void GinIndex::compact_index()
    {
        // Remove empty posting lists
        auto it = posting_lists_.begin();
        while (it != posting_lists_.end()) {
            if (it->second.row_ids.empty()) {
                it = posting_lists_.erase(it);
            } else {
                // Compress posting list if beneficial
                compress_posting_list(it->second);
                ++it;
            }
        }

        stats_dirty_ = true;
    }

    bool GinIndex::open_existing(std::uint32_t root_page)
    {
        meta_page_ = root_page;
        read_meta_page();
        return true;
    }

    void GinIndex::create_empty()
    {
        meta_page_ = 1; // Placeholder
        meta_header_ = {};
        posting_lists_.clear();
        stats_dirty_ = true;
    }

    double GinIndex::estimate_search_cost(const std::string& key) const
    {
        auto tokens = tokenize_text(key);
        if (tokens.empty()) {
            return 0.1;
        }

        // Cost is proportional to the size of the smallest posting list
        std::uint32_t min_posting_size = std::numeric_limits<std::uint32_t>::max();

        for (const auto& token : tokens) {
            auto it = posting_lists_.find(token);
            if (it == posting_lists_.end()) {
                return 0.1; // Very cheap if token doesn't exist
            }
            min_posting_size =
                std::min(min_posting_size, static_cast<std::uint32_t>(it->second.row_ids.size()));
        }

        return 1.0 + std::log(min_posting_size) / std::log(2.0); // Logarithmic cost
    }

    double GinIndex::estimate_range_cost(const std::string& lo, const std::string& hi) const
    {
        (void)lo;
        (void)hi; // Suppress warnings
        // GIN indexes don't support traditional range queries efficiently
        return std::numeric_limits<double>::infinity();
    }

    double GinIndex::estimate_maintenance_cost() const
    {
        // Cost depends on number of tokens and average posting list size
        std::uint64_t total_postings = 0;
        for (const auto& [token, entry] : posting_lists_) {
            total_postings += entry.row_ids.size();
        }
        return static_cast<double>(total_postings) / 1000.0; // Scale factor
    }

    // GIN-specific search operations
    void GinIndex::search_tokens(const std::vector<std::string>& tokens,
                                 std::vector<std::uint64_t>& out) const
    {
        if (tokens.empty()) {
            out.clear();
            return;
        }

        // Get posting lists for all tokens
        std::vector<std::vector<std::uint64_t>> posting_lists;
        for (const auto& token : tokens) {
            auto it = posting_lists_.find(normalize_token(token));
            if (it == posting_lists_.end()) {
                out.clear(); // If any token is missing, no documents match
                return;
            }

            // Decompress posting list if needed
            PostingListEntry entry_copy = it->second;
            if (entry_copy.compressed) {
                decompress_posting_list(entry_copy);
            }
            posting_lists.push_back(entry_copy.row_ids);
        }

        // Find intersection of all posting lists
        out = intersect_posting_lists(posting_lists);
    }

    void GinIndex::search_phrase(const std::string& phrase, std::vector<std::uint64_t>& out) const
    {
        // For phrase search, we need positional information
        // This is a simplified implementation that treats it as token intersection
        search_equal(phrase, out);
    }

    void GinIndex::search_boolean(const std::string& query, std::vector<std::uint64_t>& out) const
    {
        // Simplified boolean query - just tokenize and search
        // Real implementation would parse boolean operators (AND, OR, NOT)
        search_equal(query, out);
    }

    void GinIndex::search_contains_all(const std::vector<std::string>& elements,
                                       std::vector<std::uint64_t>& out) const
    {
        search_tokens(elements, out);
    }

    void GinIndex::search_contains_any(const std::vector<std::string>& elements,
                                       std::vector<std::uint64_t>& out) const
    {
        if (elements.empty()) {
            out.clear();
            return;
        }

        std::vector<std::vector<std::uint64_t>> posting_lists;
        for (const auto& element : elements) {
            auto it = posting_lists_.find(normalize_token(element));
            if (it != posting_lists_.end()) {
                // Decompress posting list if needed
                PostingListEntry entry_copy = it->second;
                if (entry_copy.compressed) {
                    decompress_posting_list(entry_copy);
                }
                posting_lists.push_back(entry_copy.row_ids);
            }
        }

        if (posting_lists.empty()) {
            out.clear();
            return;
        }

        out = union_posting_lists(posting_lists);
    }

    void GinIndex::search_overlaps(const std::vector<std::string>& elements,
                                   std::vector<std::uint64_t>& out) const
    {
        // For array overlap, any common element counts as a match
        search_contains_any(elements, out);
    }

    GinIndexStats GinIndex::compute_statistics() const
    {
        GinIndexStats stats;

        stats.total_tokens = posting_lists_.size();

        std::uint64_t total_postings = 0;
        std::uint32_t compressed_count = 0;

        for (const auto& [token, entry] : posting_lists_) {
            total_postings += entry.row_ids.size();
            if (entry.compressed) {
                compressed_count++;
            }
        }

        stats.total_postings = total_postings;
        stats.compressed_postings = compressed_count;

        if (stats.total_tokens > 0) {
            stats.average_document_length =
                static_cast<double>(total_postings) / stats.total_tokens;
        }

        if (compressed_count > 0) {
            stats.compression_ratio = 0.4; // Assume 40% compression
        }

        return stats;
    }

    void GinIndex::vacuum_posting_lists()
    {
        // Remove empty posting lists and compact data
        auto it = posting_lists_.begin();
        while (it != posting_lists_.end()) {
            if (it->second.row_ids.empty()) {
                it = posting_lists_.erase(it);
            } else {
                // Sort and deduplicate row IDs
                auto& row_ids = it->second.row_ids;
                std::sort(row_ids.begin(), row_ids.end());
                row_ids.erase(std::unique(row_ids.begin(), row_ids.end()), row_ids.end());
                it->second.frequency = row_ids.size();
                ++it;
            }
        }
        stats_dirty_ = true;
    }

    void GinIndex::reindex_tokens()
    {
        // Re-analyze all tokens and rebuild posting lists
        // This is a maintenance operation
        vacuum_posting_lists();

        for (auto& [token, entry] : posting_lists_) {
            compress_posting_list(entry);
        }
    }

    bool GinIndex::bulk_insert_documents(
        const std::vector<std::pair<std::string, std::uint64_t>>& documents, std::string& err)
    {
        try {
            for (const auto& [document, row_id] : documents) {
                if (!insert(document, row_id, err)) {
                    return false;
                }
            }
            return true;
        } catch (const std::exception& e) {
            err = "Bulk insert failed: " + std::string(e.what());
            return false;
        }
    }

    // Private implementation methods

    std::vector<std::uint8_t> GinIndex::new_page_buffer(ods::PageType type,
                                                        std::uint32_t page_no) const
    {
        (void)page_no; // Suppress warning
        std::vector<std::uint8_t> buffer(page_size_, 0);
        ods::PageHeader header;
        header.type = static_cast<std::uint16_t>(type);
        header.flags = 0;
        std::memcpy(buffer.data(), &header, sizeof(header));
        return buffer;
    }

    void GinIndex::write_page(std::uint32_t page_no, const std::vector<std::uint8_t>& page)
    {
        (void)page_no;
        (void)page; // Suppress warnings
        // In real implementation, would use fmap_ to write page
    }

    void GinIndex::read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const
    {
        (void)page_no; // Suppress warning
        // In real implementation, would use fmap_ to read page
        page = new_page_buffer(ods::PageType::GinMeta, page_no);
    }

    void GinIndex::write_meta_page()
    {
        auto page_buffer = new_page_buffer(ods::PageType::GinMeta, meta_page_);
        std::memcpy(page_buffer.data() + sizeof(ods::PageHeader), &meta_header_,
                    sizeof(meta_header_));
        write_page(meta_page_, page_buffer);
    }

    void GinIndex::read_meta_page()
    {
        std::vector<std::uint8_t> page_buffer;
        read_page(meta_page_, page_buffer);
        if (page_buffer.size() >= sizeof(ods::PageHeader) + sizeof(meta_header_)) {
            std::memcpy(&meta_header_, page_buffer.data() + sizeof(ods::PageHeader),
                        sizeof(meta_header_));
        }
    }

    bool GinIndex::token_insert(const std::string& token, std::uint64_t row_id, std::string& err)
    {
        try {
            std::string normalized = normalize_token(token);

            // Skip stop words unless configured otherwise
            if (is_stop_word(normalized)) {
                return true;
            }

            auto& posting_list = posting_lists_[normalized];

            // Add row ID if not already present
            auto it = std::find(posting_list.row_ids.begin(), posting_list.row_ids.end(), row_id);
            if (it == posting_list.row_ids.end()) {
                posting_list.row_ids.push_back(row_id);
                posting_list.frequency++;

                // Compress if posting list becomes large
                if (posting_list.row_ids.size() > tunables_.posting_list_threshold) {
                    compress_posting_list(posting_list);
                }
            }

            return true;
        } catch (const std::exception& e) {
            err = "Token insert failed: " + std::string(e.what());
            return false;
        }
    }

    bool GinIndex::token_search(const std::string& token, std::vector<std::uint64_t>& out) const
    {
        std::string normalized = normalize_token(token);

        auto it = posting_lists_.find(normalized);
        if (it == posting_lists_.end()) {
            out.clear();
            return true;
        }

        // Decompress posting list if needed for search
        PostingListEntry entry_copy = it->second;
        if (entry_copy.compressed) {
            decompress_posting_list(entry_copy);
        }

        out = entry_copy.row_ids;
        return true;
    }

    bool GinIndex::token_delete(const std::string& token, std::uint64_t row_id)
    {
        std::string normalized = normalize_token(token);

        auto it = posting_lists_.find(normalized);
        if (it == posting_lists_.end()) {
            return true; // Token doesn't exist
        }

        auto& row_ids = it->second.row_ids;
        auto row_it = std::find(row_ids.begin(), row_ids.end(), row_id);
        if (row_it != row_ids.end()) {
            row_ids.erase(row_it);
            it->second.frequency--;

            // Remove posting list if empty
            if (row_ids.empty()) {
                posting_lists_.erase(it);
            }
        }

        return true;
    }

    PostingListEntry& GinIndex::get_posting_list(const std::string& token)
    {
        return posting_lists_[normalize_token(token)];
    }

    void GinIndex::compress_posting_list(PostingListEntry& entry)
    {
        if (entry.compressed || entry.row_ids.size() < tunables_.posting_list_threshold) {
            return;
        }

        if (tunables_.compress_posting_lists) {
            // Sort row IDs for better compression
            std::sort(entry.row_ids.begin(), entry.row_ids.end());

            // Apply delta compression: store differences between consecutive IDs
            if (entry.row_ids.size() > 1) {
                std::vector<std::uint64_t> compressed_data;
                compressed_data.reserve(entry.row_ids.size());

                // Store first ID as-is
                compressed_data.push_back(entry.row_ids[0]);

                // Store deltas for subsequent IDs
                for (std::size_t i = 1; i < entry.row_ids.size(); ++i) {
                    std::uint64_t delta = entry.row_ids[i] - entry.row_ids[i - 1];
                    compressed_data.push_back(delta);
                }

                // Replace original data with compressed version
                entry.row_ids = std::move(compressed_data);
                entry.compressed = true;
            }
        }
    }

    void GinIndex::decompress_posting_list(PostingListEntry& entry) const
    {
        if (!entry.compressed) {
            return;
        }

        // Decompress delta-encoded data back to original row IDs
        if (entry.row_ids.size() > 1) {
            std::vector<std::uint64_t> decompressed_data;
            decompressed_data.reserve(entry.row_ids.size());

            // First ID is stored as-is
            std::uint64_t current_id = entry.row_ids[0];
            decompressed_data.push_back(current_id);

            // Reconstruct subsequent IDs by adding deltas
            for (std::size_t i = 1; i < entry.row_ids.size(); ++i) {
                current_id += entry.row_ids[i]; // Add delta to get original ID
                decompressed_data.push_back(current_id);
            }

            // Replace compressed data with decompressed version
            entry.row_ids = std::move(decompressed_data);
        }

        entry.compressed = false;
    }

    std::uint32_t GinIndex::allocate_posting_page()
    {
        // Simplified page allocation - would use actual page management
        static std::uint32_t next_page = 100;
        return ++next_page;
    }

    std::vector<std::uint64_t>
    GinIndex::intersect_posting_lists(const std::vector<std::vector<std::uint64_t>>& lists) const
    {
        if (lists.empty()) {
            return {};
        }

        if (lists.size() == 1) {
            return lists[0];
        }

        // Start with the smallest list for efficiency
        auto result =
            *std::min_element(lists.begin(), lists.end(),
                              [](const auto& a, const auto& b) { return a.size() < b.size(); });

        // Intersect with each remaining list
        for (const auto& list : lists) {
            if (list == result)
                continue; // Skip the initial list

            std::vector<std::uint64_t> intersection;
            std::set_intersection(result.begin(), result.end(), list.begin(), list.end(),
                                  std::back_inserter(intersection));
            result = std::move(intersection);

            if (result.empty())
                break; // Early termination
        }

        return result;
    }

    std::vector<std::uint64_t>
    GinIndex::union_posting_lists(const std::vector<std::vector<std::uint64_t>>& lists) const
    {
        if (lists.empty()) {
            return {};
        }

        std::set<std::uint64_t> result_set;
        for (const auto& list : lists) {
            result_set.insert(list.begin(), list.end());
        }

        return std::vector<std::uint64_t>(result_set.begin(), result_set.end());
    }

    std::vector<std::string> GinIndex::tokenize_text(const std::string& text) const
    {
        return tokenizer_->tokenize(text);
    }

    std::string GinIndex::normalize_token(const std::string& token) const
    {
        std::string normalized = token;
        if (!tunables_.case_sensitive) {
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        }
        return normalized;
    }

    std::vector<std::uint8_t> GinIndex::compress_row_ids(const std::vector<std::uint64_t>& row_ids)
    {
        // Simplified delta compression
        std::vector<std::uint8_t> compressed;

        if (row_ids.empty())
            return compressed;

        // Sort for better compression
        auto sorted = row_ids;
        std::sort(sorted.begin(), sorted.end());

        // Delta encode (simplified - real implementation would use variable-length encoding)
        std::uint64_t prev = 0;
        for (std::uint64_t id : sorted) {
            std::uint64_t delta = id - prev;

            // Simple byte encoding (real implementation would be more sophisticated)
            while (delta > 0) {
                compressed.push_back(static_cast<std::uint8_t>(delta & 0xFF));
                delta >>= 8;
            }
            compressed.push_back(0); // Delimiter

            prev = id;
        }

        return compressed;
    }

    std::vector<std::uint64_t>
    GinIndex::decompress_row_ids(const std::vector<std::uint8_t>& compressed)
    {
        // Simplified delta decompression
        std::vector<std::uint64_t> row_ids;

        std::uint64_t current = 0;
        std::uint64_t delta = 0;
        int shift = 0;

        for (std::uint8_t byte : compressed) {
            if (byte == 0) { // Delimiter
                current += delta;
                row_ids.push_back(current);
                delta = 0;
                shift = 0;
            } else {
                delta |= (static_cast<std::uint64_t>(byte) << shift);
                shift += 8;
            }
        }

        return row_ids;
    }

    bool GinIndex::is_stop_word(const std::string& token) const
    {
        return stop_words_.find(token) != stop_words_.end();
    }

    double GinIndex::calculate_idf(const std::string& token) const
    {
        auto it = posting_lists_.find(token);
        if (it == posting_lists_.end()) {
            return 0.0;
        }

        // IDF = log(total_documents / documents_with_term)
        std::uint32_t docs_with_term = it->second.frequency;
        if (docs_with_term == 0)
            return 0.0;

        // Simplified total document count
        std::uint32_t total_docs = std::max(1u, static_cast<std::uint32_t>(posting_lists_.size()));

        return std::log(static_cast<double>(total_docs) / docs_with_term);
    }

} // namespace scratchbird::engine
