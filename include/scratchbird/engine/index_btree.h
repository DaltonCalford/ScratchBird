#ifndef SCRATCHBIRD_ENGINE_INDEX_BTREE_H
#define SCRATCHBIRD_ENGINE_INDEX_BTREE_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    struct BTreeTunables {
        double fillfactor{0.70};
        enum class SplitPolicy { Even, LeftBiased, RightBiased } split_policy{SplitPolicy::Even};
        std::uint32_t prefetch_horizon_pages{0};
        bool enable_key_prefix_compare{true};
        bool enable_leaf_prefix_compression{false};
    };

    struct BTreeKeyRef {
        std::string key;         // normalized key bytes (collation-applied)
        std::uint64_t row_id{0}; // placeholder record reference
    };

    struct BTreeBuildResult {
        std::uint32_t root_page{0};
        std::uint32_t leaf_pages{0};
        std::uint32_t key_count{0};
    };

    class BTreeBuilder
    {
      public:
        BTreeBuilder(FileMap fmap, std::uint32_t page_size);
        BTreeBuildResult build(const std::vector<BTreeKeyRef>& keys);

      private:
        void write_leaf_page(std::uint32_t page_no, const std::vector<BTreeKeyRef>& slice);
        void write_root(const std::vector<std::uint32_t>& child_pages, std::uint32_t root_page);
        FileMap fmap_;
        std::uint32_t page_size_{4096};
        std::uint32_t next_alloc_page_{0};
    };

    struct BTreeStats {
        std::uint32_t height{0};
        std::uint32_t leaf_pages{0};
        std::uint32_t branch_pages{0};
        std::uint64_t key_count{0};
        std::string min_key;
        std::string max_key;
    };

    class BTreeIndex
    {
      public:
        BTreeIndex(FileMap fmap, std::uint32_t page_size, bool unique);
        void set_tunables(const BTreeTunables& t)
        {
            tunables_ = t;
        }
        void create_empty();
        bool insert(const std::string& key, std::uint64_t row_id, std::string& err);
        // Insert with payload (INCLUDE-like). Payload stored verbatim; not part of ordering.
        bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                                 const std::string& payload, std::string& err);

        void search_equal(const std::string& key, std::vector<std::uint64_t>& out) const;
        void
        search_equal_with_payload(const std::string& key,
                                  std::vector<std::pair<std::uint64_t, std::string>>& out) const;
        void search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                          std::vector<std::pair<std::string, std::uint64_t>>& out) const;
        std::size_t erase_equal(const std::string& key, std::string& err);

        // Stats and maintenance
        BTreeStats compute_stats() const;
        void rebuild_offline();

        // Validation walker: verifies leaf key ordering, sibling links, and basic root-child
        // coverage. Returns true if valid; on failure sets error with first offending
        // page/condition.
        bool validate(std::string& error) const;

        // Open an existing index by setting root page and loading children from root.
        bool open_existing(std::uint32_t root_page)
        {
            root_page_ = root_page;
            read_root();
            return true;
        }

        std::uint32_t root_page() const
        {
            return root_page_;
        }

      private:
        void maybe_prefetch(std::uint32_t page_no) const;
        struct RootChild {
            std::string min_key;
            std::uint32_t page_no{0};
        };
        struct LeafHdr {
            std::uint16_t num_slots;
            std::uint16_t free_start;
            std::uint16_t dir_start;
            std::uint16_t reserved;
        };

        std::vector<std::uint8_t> new_page_buffer(ods::PageType t, std::uint32_t page_no) const;
        void write_page(std::uint32_t page_no, std::vector<std::uint8_t>& page);
        void read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const;
        static LeafHdr read_leaf_hdr(const std::vector<std::uint8_t>& page);
        static void write_leaf_hdr(std::vector<std::uint8_t>& page, const LeafHdr& h);
        static std::uint16_t read_slot(const std::vector<std::uint8_t>& page, const LeafHdr& h,
                                       int index);
        static void write_slot(std::vector<std::uint8_t>& page, const LeafHdr& h, int index,
                               std::uint16_t value);
        static int keycmp(const std::string& a, const std::string& b);

        // record layout helpers
        static void write_record(std::vector<std::uint8_t>& page, std::uint16_t& at,
                                 const std::string& key, std::uint64_t row_id,
                                 const std::string& payload);
        static void read_record(const std::vector<std::uint8_t>& page, std::uint16_t off,
                                std::string& key, std::uint64_t& row_id, std::string& payload);
        static std::uint16_t record_size(const std::string& key, const std::string& payload);

        // Leaf helpers
        bool leaf_insert_common(std::uint32_t page_no, const std::string& key, std::uint64_t row_id,
                                const std::string& payload, std::string& err,
                                std::string* split_key, std::uint32_t* new_page_out);
        bool leaf_insert(std::uint32_t page_no, const std::string& key, std::uint64_t row_id,
                         std::string& err, std::string* split_key, std::uint32_t* new_page_out);
        void leaf_scan_equal(std::uint32_t page_no, const std::string& key,
                             std::vector<std::uint64_t>& out) const;
        void leaf_scan_equal_payload(std::uint32_t page_no, const std::string& key,
                                     std::vector<std::pair<std::uint64_t, std::string>>& out) const;
        void leaf_scan_range(std::uint32_t page_no, const std::string& lo, bool lo_incl,
                             const std::string& hi, bool hi_incl,
                             std::vector<std::pair<std::string, std::uint64_t>>& out) const;
        bool leaf_erase_rewrite(std::uint32_t page_no, const std::string& key, std::size_t& removed,
                                std::string& new_first_key);
        bool try_merge_with_right(std::size_t child_index);
        std::string read_first_key(std::uint32_t page_no) const;

        // Root helpers
        int find_child_for_key(const std::string& key) const;
        void write_root();
        void read_root();
        void insert_root_child(int after_index, const std::string& split_key,
                               std::uint32_t new_page);

        FileMap fmap_;
        std::uint32_t page_size_{4096};
        std::uint32_t next_alloc_page_{0};
        bool unique_{false};
        BTreeTunables tunables_{};
        std::uint32_t root_page_{0};
        std::vector<RootChild> children_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_BTREE_H
