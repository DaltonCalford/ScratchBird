#ifndef SCRATCHBIRD_ENGINE_INDEX_GIN_H
#define SCRATCHBIRD_ENGINE_INDEX_GIN_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    /**
     * GIN index tunables for performance optimization
     */
    struct GinIndexTunables {
        std::uint32_t posting_list_threshold{100}; // Split posting list when it exceeds this size
        bool compress_posting_lists{true};         // Compress large posting lists
        std::string tokenizer{"simple"};     // Tokenization strategy: simple, whitespace, etc.
        std::uint32_t max_token_length{256}; // Maximum token length
        std::uint32_t gin_cache_size{1000};  // Number of posting lists to cache
        bool case_sensitive{false};          // Whether tokens are case sensitive
    };

    /**
     * GIN meta page header structure
     */
    struct GinMetaHeader {
        std::uint32_t total_tokens{0};      // Total number of distinct tokens
        std::uint32_t total_entries{0};     // Total number of index entries
        std::uint32_t root_posting_tree{0}; // Root of B-tree for posting lists
        std::uint32_t tokenizer_version{1}; // Version of tokenizer used
        std::uint32_t flags{0};             // Configuration flags
    };

    /**
     * GIN posting list entry
     */
    struct PostingListEntry {
        std::vector<std::uint64_t> row_ids; // Row IDs containing this token
        bool compressed{false};             // Whether row IDs are compressed
        std::uint32_t frequency{0};         // Number of occurrences
    };

    /**
     * Token information
     */
    struct TokenInfo {
        std::string token;               // The token string
        std::uint32_t frequency{0};      // How many documents contain this token
        std::uint32_t posting_page{0};   // Page containing posting list
        std::uint16_t posting_offset{0}; // Offset within page
    };

    /**
     * GIN index statistics for monitoring and optimization
     */
    struct GinIndexStats {
        std::uint32_t total_tokens{0};        // Number of distinct tokens
        std::uint32_t total_documents{0};     // Number of indexed documents
        std::uint64_t total_postings{0};      // Total posting list entries
        std::uint32_t compressed_postings{0}; // Number of compressed posting lists
        double average_document_length{0.0};  // Average tokens per document
        double compression_ratio{1.0};        // Posting list compression ratio
        std::vector<std::uint32_t> token_frequency_distribution; // Histogram of token frequencies
    };

    /**
     * Text tokenizer interface
     */
    class Tokenizer
    {
      public:
        virtual ~Tokenizer() = default;
        virtual std::vector<std::string> tokenize(const std::string& text) = 0;
        virtual std::string name() const = 0;
    };

    /**
     * Simple whitespace tokenizer
     */
    class SimpleTokenizer : public Tokenizer
    {
      public:
        SimpleTokenizer(bool case_sensitive = false, std::uint32_t max_length = 256)
            : case_sensitive_(case_sensitive), max_token_length_(max_length)
        {
        }

        std::vector<std::string> tokenize(const std::string& text) override;
        std::string name() const override
        {
            return "simple";
        }

      private:
        bool case_sensitive_;
        std::uint32_t max_token_length_;
    };

    /**
     * GIN (Generalized Inverted) Index implementation
     * Optimized for full-text search and array/JSONB containment queries
     */
    class GinIndex : public IndexFamily
    {
      public:
        GinIndex(FileMap fmap, std::uint32_t page_size, bool unique);
        ~GinIndex() override = default;

        void set_tunables(const GinIndexTunables& tunables)
        {
            tunables_ = tunables;
        }
        void set_tokenizer(std::unique_ptr<Tokenizer> tokenizer)
        {
            tokenizer_ = std::move(tokenizer);
        }

        // IndexFamily interface
        bool insert(const std::string& key, std::uint64_t row_id, std::string& err) override;
        bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                                 const std::string& payload, std::string& err) override;
        void search_equal(const std::string& key, std::vector<std::uint64_t>& out) const override;
        void search_equal_with_payload(
            const std::string& key,
            std::vector<std::pair<std::uint64_t, std::string>>& out) const override;
        void search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                          std::vector<std::pair<std::string, std::uint64_t>>& out) const override;
        std::size_t erase_equal(const std::string& key, std::string& err) override;

        bool validate(std::string& error) const override;
        void rebuild_offline() override;
        std::string collect_statistics() const override;
        void compact_index() override;

        IndexMethod get_method() const override
        {
            return IndexMethod::Gin;
        }
        std::uint32_t root_page() const override
        {
            return meta_page_;
        }
        bool open_existing(std::uint32_t root_page) override;
        void create_empty() override;

        // Cost estimation
        double estimate_search_cost(const std::string& key) const override;
        double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
        double estimate_maintenance_cost() const override;

        // GIN-specific operations

        // Full-text search operations
        void search_tokens(const std::vector<std::string>& tokens,
                           std::vector<std::uint64_t>& out) const;
        void search_phrase(const std::string& phrase, std::vector<std::uint64_t>& out) const;
        void search_boolean(const std::string& query, std::vector<std::uint64_t>& out) const;

        // Array/JSONB operations
        void search_contains_all(const std::vector<std::string>& elements,
                                 std::vector<std::uint64_t>& out) const;
        void search_contains_any(const std::vector<std::string>& elements,
                                 std::vector<std::uint64_t>& out) const;
        void search_overlaps(const std::vector<std::string>& elements,
                             std::vector<std::uint64_t>& out) const;

        // Statistics and maintenance
        GinIndexStats compute_statistics() const;
        void vacuum_posting_lists();
        void reindex_tokens();

        // Bulk operations
        bool
        bulk_insert_documents(const std::vector<std::pair<std::string, std::uint64_t>>& documents,
                              std::string& err);

      private:
        // Page management
        std::vector<std::uint8_t> new_page_buffer(ods::PageType type, std::uint32_t page_no) const;
        void write_page(std::uint32_t page_no, const std::vector<std::uint8_t>& page);
        void read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const;

        // Meta page operations
        void write_meta_page();
        void read_meta_page();

        // Token operations
        bool token_insert(const std::string& token, std::uint64_t row_id, std::string& err);
        bool token_search(const std::string& token, std::vector<std::uint64_t>& out) const;
        bool token_delete(const std::string& token, std::uint64_t row_id);

        // Posting list operations
        PostingListEntry& get_posting_list(const std::string& token);
        void compress_posting_list(PostingListEntry& entry);
        void decompress_posting_list(PostingListEntry& entry);
        std::uint32_t allocate_posting_page();

        // Set operations for combining posting lists
        std::vector<std::uint64_t>
        intersect_posting_lists(const std::vector<std::vector<std::uint64_t>>& lists) const;
        std::vector<std::uint64_t>
        union_posting_lists(const std::vector<std::vector<std::uint64_t>>& lists) const;

        // Tokenization and text processing
        std::vector<std::string> tokenize_text(const std::string& text) const;
        std::string normalize_token(const std::string& token) const;

        // Posting list compression (delta encoding)
        std::vector<std::uint8_t> compress_row_ids(const std::vector<std::uint64_t>& row_ids);
        std::vector<std::uint64_t> decompress_row_ids(const std::vector<std::uint8_t>& compressed);

        // Utility functions
        bool is_stop_word(const std::string& token) const;
        double calculate_idf(const std::string& token) const; // Inverse document frequency

        FileMap fmap_;
        std::uint32_t page_size_{4096};
        bool unique_{false}; // GIN indexes are typically not unique
        GinIndexTunables tunables_;
        std::unique_ptr<Tokenizer> tokenizer_;

        // Index structure
        std::uint32_t meta_page_{0};
        GinMetaHeader meta_header_;

        // In-memory structures (would be persistent in real implementation)
        std::unordered_map<std::string, PostingListEntry> posting_lists_;
        std::unordered_set<std::string> stop_words_;

        // Statistics
        mutable GinIndexStats cached_stats_;
        mutable bool stats_dirty_{true};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_GIN_H
