#ifndef SCRATCHBIRD_ENGINE_BTREE_PAGE_H
#define SCRATCHBIRD_ENGINE_BTREE_PAGE_H

#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    // Composite key parts with NULL and direction flags.
    struct KeyPart {
        std::string bytes;
        bool is_null{false};
        bool desc{false};
    };

    struct CompositeKey {
        std::vector<KeyPart> parts;
    };

    // Leaf record: composite key + row_id + optional payload (INCLUDE data)
    struct LeafRecordV1 {
        CompositeKey key;
        std::uint64_t row_id{0};
        std::string payload; // opaque include payload
    };

    // Branch entry: separator key and child page pointer
    struct BranchEntryV1 {
        CompositeKey sep_key; // minimal separator (<= first key in child)
        std::uint32_t child_page{0};
    };

    enum class BTreePageFlags : std::uint16_t {
        None = 0,
        LeafPerPagePrefix = 1 << 0,
        BranchPerPagePrefix = 1 << 1
    };

    inline BTreePageFlags operator|(BTreePageFlags a, BTreePageFlags b)
    {
        return static_cast<BTreePageFlags>(static_cast<std::uint16_t>(a) |
                                           static_cast<std::uint16_t>(b));
    }

    // In-page B-Tree header immediately after ODS PageHeader
    struct BTreeHdrV1 {
        std::uint16_t num_slots{0};      // entries in slot dir
        std::uint16_t free_start{0};     // first free byte in growing-up region
        std::uint16_t dir_start{0};      // start of slot dir (grows down)
        std::uint16_t flags{0};          // BTreePageFlags
        std::uint16_t high_key_off{0};   // offset to high-key (fence key), 0 if none
        std::uint16_t prefix_off{0};     // offset to per-page prefix, 0 if none
        std::uint32_t leftmost_child{0}; // for branch pages: page id of leftmost child
    };

    // Encoder/decoder for composite keys (v1)
    namespace detail
    {
        // Encoding: [u16 num_parts] { [u8 flags] [u16 len] [bytes] }*
        // flags: bit0 = is_null, bit1 = desc
        void encode_key(const CompositeKey& key, std::vector<std::uint8_t>& out);
        void decode_key(const std::uint8_t* data, std::size_t len, CompositeKey& out);
    } // namespace detail

    // Build a leaf page image (v1). Inputs must be sorted by key asc per flags.
    // - page buffer must be pre-sized to page_size and zeroed by caller
    // - prev/next (siblings) and page_no are set in ODS header by caller
    // - scn acts as per-page LSN (set by caller)
    // Options:
    // - per_page_prefix: optional string placed once and used as common prefix
    void build_leaf_page_v1(std::vector<std::uint8_t>& page, std::uint32_t page_size,
                            const std::vector<LeafRecordV1>& records, const CompositeKey* high_key,
                            const std::string* per_page_prefix);

    // Parse leaf page (v1) and return records, high_key, per_page_prefix (if present)
    void parse_leaf_page_v1(const std::vector<std::uint8_t>& page,
                            std::vector<LeafRecordV1>& out_records, CompositeKey& out_high_key,
                            std::string& out_per_page_prefix);

    // Build a branch page image (v1)
    void build_branch_page_v1(std::vector<std::uint8_t>& page, std::uint32_t page_size,
                              const std::vector<BranchEntryV1>& entries,
                              std::uint32_t leftmost_child, const CompositeKey* high_key,
                              const std::string* per_page_prefix);

    // Parse branch page (v1)
    void parse_branch_page_v1(const std::vector<std::uint8_t>& page,
                              std::vector<BranchEntryV1>& out_entries, CompositeKey& out_high_key,
                              std::string& out_per_page_prefix, std::uint32_t& out_leftmost_child);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_BTREE_PAGE_H
