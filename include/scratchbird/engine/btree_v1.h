#ifndef SCRATCHBIRD_ENGINE_BTREE_V1_H
#define SCRATCHBIRD_ENGINE_BTREE_V1_H

#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/btree_page.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/latch.h"
#include "scratchbird/engine/pager.h"
#include "scratchbird/engine/resolver.h"
#include "scratchbird/engine/wal.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    class BTreeV1
    {
      public:
        BTreeV1(FileMap fmap, std::uint32_t page_size, bool unique);
        void set_wal(WalManager* wal)
        {
            wal_ = wal;
        }

        struct FieldOrder {
            bool desc{false};
            bool nulls_first{true};
            bool case_insensitive{false};
            std::string collate; // reserved for future ICU
        };
        // Configure per-column ordering/flags (size must match key arity)
        void set_field_orders(std::vector<FieldOrder> orders)
        {
            field_orders_ = std::move(orders);
        }
        // Include column count hint (for payload encoding outside this class)
        void set_include_count(std::size_t count)
        {
            include_count_ = count;
        }
        // Partial index predicate: if set and returns false, insert/erase are no-ops
        using PredicateFn =
            std::function<bool(const CompositeKey&, std::uint64_t, const std::string&)>;
        void set_predicate(PredicateFn pred)
        {
            predicate_ = std::move(pred);
        }

        void create_empty(); // single empty leaf root

        bool insert(const CompositeKey& key, std::uint64_t row_id, const std::string& payload,
                    std::string& err);
        std::size_t erase_equal(const CompositeKey& key); // simple leaf erase

        // In-order scan (keys only) using sibling next pointers; returns concatenated keys as
        // strings (joined parts with '|')
        void inorder_keys(std::vector<std::string>& out) const;

        std::uint32_t root_page() const
        {
            return root_page_;
        }
        bool root_is_leaf() const
        {
            return root_is_leaf_;
        }

      private:
        struct InsertUp {
            bool has{false};
            CompositeKey sep;
            std::uint32_t right_child{0};
        };

        // IO helpers
        void read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const;
        void write_page(std::uint32_t page_no, std::vector<std::uint8_t>& page);
        std::uint32_t alloc_page();

        // Tree ops
        InsertUp insert_recursive(std::uint32_t page_no, bool is_leaf, const CompositeKey& key,
                                  std::uint64_t row_id, const std::string& payload,
                                  std::string& err);
        InsertUp insert_into_leaf(std::uint32_t page_no, const CompositeKey& key,
                                  std::uint64_t row_id, const std::string& payload);
        InsertUp insert_into_branch(std::uint32_t page_no, const CompositeKey& sep,
                                    std::uint32_t child_page);

        int cmp_key(const CompositeKey& a, const CompositeKey& b) const;
        static std::string key_to_string(const CompositeKey& k);

        FileMap fmap_;
        std::uint32_t page_size_{4096};
        std::shared_ptr<BufferCache> cache_{};
        mutable Pager pager_;
        Allocator allocator_;
        bool unique_{false};
        std::vector<FieldOrder> field_orders_{};
        std::size_t include_count_{0};
        PredicateFn predicate_{};
        std::uint32_t root_page_{0};
        bool root_is_leaf_{true};
        mutable LatchManager latches_{};
        WalManager* wal_{nullptr};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_BTREE_V1_H
