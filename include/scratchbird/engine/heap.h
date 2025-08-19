#ifndef SCRATCHBIRD_ENGINE_HEAP_H
#define SCRATCHBIRD_ENGINE_HEAP_H

#include "scratchbird/engine/ods.h"
#include "scratchbird/engine/txn.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    struct HeapPageFlagBits {
        static constexpr std::uint16_t HasFreeSpace = 1u << 0;
        static constexpr std::uint16_t HasOverflow = 1u << 1;
    };

    enum class AttrType { Int64, VarBytes };
    using TypeId = AttrType;

    struct AttrMeta {
        TypeId type{TypeId::Int64};
        std::uint16_t fixed_len{0};
        bool by_val{true};
        bool nullable{false};
    };

    struct TupleLayout {
        std::vector<AttrMeta> attrs;
    };

    struct Value {
        bool is_null{false};
        std::uint64_t u64{0};
        std::string bytes{};
    };

    // Forward decls
    struct SnapshotRC;
    enum class TxnState : std::uint8_t;
    // Visibility helper (RC)
    inline bool is_visible_rc(const SnapshotRC& snap, const ods::TupleHeader& th,
                              const std::function<TxnState(std::uint64_t)>& read_state)
    {
        // Legacy rows (Phase 1) without MGA fields are visible
        if (th.created_xid == 0)
            return true;
        // Read-your-writes for inserts: visible unless deleted by self
        if (th.created_xid == snap.own_xid) {
            return th.deleted_xid != snap.own_xid;
        }
        // Creator must be committed for others
        if (read_state(th.created_xid) != TxnState::Committed)
            return false;
        // Deletions:
        // - Hide if deleted by self
        // - Hide if deleted by a committed txn
        // - Otherwise (deleter uncommitted/aborted), keep visible to others
        if (th.deleted_xid != 0) {
            if (th.deleted_xid == snap.own_xid)
                return false;
            if (read_state(th.deleted_xid) == TxnState::Committed)
                return false;
            // deleter not committed → still visible to others
            return true;
        }
        return true;
    }

    // Repeatable Read: Visibility is frozen at snapshot cutoff
    // SnapshotRR moved to txn.h; forward-declare here
    struct SnapshotRR;

    inline bool is_visible_rr(const SnapshotRR& snap, const ods::TupleHeader& th,
                              const std::function<TxnState(std::uint64_t)>& read_state)
    {
        if (th.created_xid == 0)
            return true;
        if (th.created_xid == snap.own_xid)
            return th.deleted_xid != snap.own_xid;
        // must have been committed and not after cutoff
        if (read_state(th.created_xid) != TxnState::Committed)
            return false;
        if (th.created_xid > snap.cutoff_committed_id)
            return false;
        if (th.deleted_xid != 0) {
            if (th.deleted_xid == snap.own_xid)
                return false;
            if (read_state(th.deleted_xid) == TxnState::Committed &&
                th.deleted_xid <= snap.cutoff_committed_id)
                return false;
        }
        return true;
    }

    struct HeapOptions {
        std::uint32_t free_space_threshold_bytes{128};
        std::uint32_t overflow_threshold_pct{ods::HEAP_OVERFLOW_THRESHOLD_PCT};
    };

    inline std::uint32_t compute_layout_format_id(const TupleLayout& layout)
    {
        // Simple FNV-1a 32-bit over AttrMeta fields
        std::uint32_t hash = 2166136261u;
        auto mix = [&](std::uint32_t v) {
            hash ^= v;
            hash *= 16777619u;
        };
        for (const auto& a : layout.attrs) {
            mix(static_cast<std::uint32_t>(a.type));
            mix(static_cast<std::uint32_t>(a.fixed_len));
            mix(a.by_val ? 1u : 0u);
            mix(a.nullable ? 1u : 0u);
        }
        if (hash == 0)
            hash = 1; // avoid 0 sentinel
        return hash;
    }

    class HeapTupleCodec
    {
      public:
        // Encode fixed-width-only tuple (Int64) with nullmap.
        static std::vector<std::uint8_t> encode_tuple(const TupleLayout& layout,
                                                      const std::vector<Value>& values)
        {
            if (values.size() != layout.attrs.size())
                throw std::runtime_error("encode_tuple: arity mismatch");
            ods::TupleHeader th{};
            th.num_attrs = static_cast<std::uint16_t>(layout.attrs.size());
            th.nullmap_bytes = static_cast<std::uint16_t>((th.num_attrs + 7) / 8);
            th.varlena_bytes = 0;
            th.flags = 0;
            const std::size_t header_size = sizeof(ods::TupleHeader);
            std::vector<std::uint8_t> out;
            out.resize(header_size + th.nullmap_bytes);
            // nullmap
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (values[i].is_null)
                    out[header_size + (i / 8)] |= static_cast<std::uint8_t>(1u << (i % 8));
            }
            // attribute directory: 2 bytes per attribute offset from tuple start (after
            // header+nullmap)
            std::size_t dir_pos = out.size();
            out.resize(dir_pos + 2 * layout.attrs.size());

            // fixed/varlen values; track offsets in attribute directory
            std::size_t data_start = out.size();
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (values[i].is_null)
                    continue;
                if (layout.attrs[i].type == AttrType::Int64) {
                    std::uint64_t v = values[i].u64;
                    std::size_t p = out.size();
                    std::uint16_t rel_off = static_cast<std::uint16_t>(
                        p - (header_size + th.nullmap_bytes + 2 * layout.attrs.size()));
                    std::memcpy(out.data() + dir_pos + 2 * i, &rel_off, 2);
                    out.resize(p + sizeof(std::uint64_t));
                    std::memcpy(out.data() + p, &v, sizeof v);
                } else if (layout.attrs[i].type == AttrType::VarBytes) {
                    const auto& s = values[i].bytes;
                    std::uint16_t len =
                        static_cast<std::uint16_t>(std::min<std::size_t>(s.size(), 0xFFFE));
                    std::size_t p = out.size();
                    std::uint16_t rel_off = static_cast<std::uint16_t>(
                        p - (header_size + th.nullmap_bytes + 2 * layout.attrs.size()));
                    std::memcpy(out.data() + dir_pos + 2 * i, &rel_off, 2);
                    out.resize(p + 2 + len);
                    std::memcpy(out.data() + p, &len, 2);
                    p += 2;
                    if (len)
                        std::memcpy(out.data() + p, s.data(), len);
                } else {
                    throw std::runtime_error("unsupported type");
                }
            }
            // finalize header
            std::memcpy(out.data(), &th, sizeof th);
            return out;
        }

        static bool decode_tuple(
            const TupleLayout& layout, const std::vector<std::uint8_t>& page, std::uint16_t off,
            std::vector<Value>& out,
            const std::function<bool(const ods::OverflowRef&, std::string&)>& overflow_reader = {})
        {
            if (off + sizeof(ods::TupleHeader) > page.size())
                return false;
            ods::TupleHeader th{};
            std::memcpy(&th, page.data() + off, sizeof th);
            if (th.num_attrs != layout.attrs.size())
                return false;
            if (off + sizeof(ods::TupleHeader) + th.nullmap_bytes > page.size())
                return false;
            const std::uint8_t* nullmap = page.data() + off + sizeof(ods::TupleHeader);
            const std::uint8_t* attr_dir = nullmap + th.nullmap_bytes;
            out.assign(layout.attrs.size(), {});
            std::size_t base =
                off + sizeof(ods::TupleHeader) + th.nullmap_bytes + 2 * layout.attrs.size();
            for (std::size_t i = 0; i < layout.attrs.size(); ++i) {
                bool is_null = (nullmap[i / 8] >> (i % 8)) & 1u;
                out[i].is_null = is_null;
                if (is_null)
                    continue;
                if (layout.attrs[i].type == AttrType::Int64) {
                    std::uint16_t rel_off = 0;
                    std::memcpy(&rel_off, attr_dir + 2 * i, 2);
                    std::size_t p = base + rel_off;
                    if (p + sizeof(std::uint64_t) > page.size())
                        return false;
                    std::uint64_t v = 0;
                    std::memcpy(&v, page.data() + p, sizeof v);
                    out[i].u64 = v;
                } else if (layout.attrs[i].type == AttrType::VarBytes) {
                    std::uint16_t rel_off = 0;
                    std::memcpy(&rel_off, attr_dir + 2 * i, 2);
                    std::size_t p = base + rel_off;
                    if (p + 2 > page.size())
                        return false;
                    std::uint16_t len = 0;
                    std::memcpy(&len, page.data() + p, 2);
                    p += 2;
                    if (len == ods::HEAP_VARLEN_LARGE_SENTINEL) {
                        if (p + sizeof(ods::OverflowRef) > page.size())
                            return false;
                        ods::OverflowRef ref{};
                        std::memcpy(&ref, page.data() + p, sizeof ref);
                        p += sizeof ref;
                        if (!overflow_reader)
                            return false;
                        if (!overflow_reader(ref, out[i].bytes))
                            return false;
                    } else {
                        if (p + len > page.size())
                            return false;
                        out[i].bytes.assign(reinterpret_cast<const char*>(page.data() + p), len);
                        p += len;
                    }
                } else {
                    return false;
                }
            }
            return true;
        }

        // Encode with overflow handling: write overflow via callback returning new page number
        static std::vector<std::uint8_t> encode_tuple_with_overflow(
            const TupleLayout& layout, const std::vector<Value>& values, std::uint32_t page_size,
            const std::function<std::uint32_t(const std::uint8_t*, std::size_t)>& write_overflow,
            std::uint32_t overflow_threshold_pct = ods::HEAP_OVERFLOW_THRESHOLD_PCT,
            std::uint32_t safety_margin = 128, std::uint64_t created_xid = 0)
        {
            if (values.size() != layout.attrs.size())
                throw std::runtime_error("encode_tuple: arity mismatch");
            ods::TupleHeader th{};
            th.num_attrs = static_cast<std::uint16_t>(layout.attrs.size());
            th.nullmap_bytes = static_cast<std::uint16_t>((th.num_attrs + 7) / 8);
            th.varlena_bytes = 0;
            th.flags = 0;
            const std::size_t header_size = sizeof(ods::TupleHeader);
            std::vector<std::uint8_t> out(header_size + th.nullmap_bytes);
            // nullmap
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (values[i].is_null)
                    out[header_size + (i / 8)] |= static_cast<std::uint8_t>(1u << (i % 8));
            }
            // attribute directory
            std::size_t dir_pos = out.size();
            out.resize(dir_pos + 2 * layout.attrs.size());

            // Compute inline threshold as min(percent of page size, page_size minus estimated
            // overheads)
            std::size_t pct_thresh = static_cast<std::size_t>(
                (static_cast<std::uint64_t>(page_size) * overflow_threshold_pct) / 100);
            std::size_t est_overhead = sizeof(ods::TupleHeader) + th.nullmap_bytes +
                                       2 * layout.attrs.size() + safety_margin;
            std::size_t abs_thresh = page_size > est_overhead ? (page_size - est_overhead) : 0;
            std::size_t inline_thresh = std::min(pct_thresh, abs_thresh);
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (values[i].is_null)
                    continue;
                if (layout.attrs[i].type == AttrType::Int64) {
                    std::uint64_t v = values[i].u64;
                    std::size_t p = out.size();
                    std::uint16_t rel_off = static_cast<std::uint16_t>(
                        p - (header_size + th.nullmap_bytes + 2 * layout.attrs.size()));
                    std::memcpy(out.data() + dir_pos + 2 * i, &rel_off, 2);
                    out.resize(p + sizeof v);
                    std::memcpy(out.data() + p, &v, sizeof v);
                } else if (layout.attrs[i].type == AttrType::VarBytes) {
                    const auto& s = values[i].bytes;
                    if (s.size() + 2 <= inline_thresh) {
                        std::uint16_t len =
                            static_cast<std::uint16_t>(std::min<std::size_t>(s.size(), 0xFFFE));
                        std::size_t p = out.size();
                        std::uint16_t rel_off = static_cast<std::uint16_t>(
                            p - (header_size + th.nullmap_bytes + 2 * layout.attrs.size()));
                        std::memcpy(out.data() + dir_pos + 2 * i, &rel_off, 2);
                        out.resize(p + 2 + len);
                        std::memcpy(out.data() + p, &len, 2);
                        p += 2;
                        if (len)
                            std::memcpy(out.data() + p, s.data(), len);
                    } else {
                        // overflow path
                        auto page_no = write_overflow(
                            reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
                        ods::OverflowRef ref{};
                        ref.page_no = page_no;
                        ref.space_id = 1;
                        ref.slot_or_off = 0;
                        ref.length = static_cast<std::uint32_t>(s.size());
                        std::size_t p = out.size();
                        std::uint16_t rel_off = static_cast<std::uint16_t>(
                            p - (header_size + th.nullmap_bytes + 2 * layout.attrs.size()));
                        std::memcpy(out.data() + dir_pos + 2 * i, &rel_off, 2);
                        out.resize(p + 2 + sizeof(ref));
                        std::uint16_t sentinel =
                            static_cast<std::uint16_t>(ods::HEAP_VARLEN_LARGE_SENTINEL);
                        std::memcpy(out.data() + p, &sentinel, 2);
                        p += 2;
                        std::memcpy(out.data() + p, &ref, sizeof ref);
                        th.flags |= 1; // has_overflow
                    }
                } else {
                    throw std::runtime_error("unsupported type");
                }
            }
            // Set transactional header fields if provided
            th.created_xid = created_xid;
            std::memcpy(out.data(), &th, sizeof th);
            return out;
        }
    };

    // Helper to compute capacity and common offsets inside a heap data page.
    struct HeapLayout {
        static std::uint32_t page_header_size()
        {
            return static_cast<std::uint32_t>(sizeof(ods::PageHeader));
        }
        static std::uint32_t heap_header_size()
        {
            return static_cast<std::uint32_t>(sizeof(ods::HeapPageHeader));
        }
        static std::uint32_t tuples_region_start()
        {
            return page_header_size() + heap_header_size();
        }
    };

    class HeapPageCodec
    {
      public:
        // Initialize an empty heap data page in the given buffer. Buffer's size defines page size.
        static void init_heap_data_page(std::vector<std::uint8_t>& page)
        {
            std::fill(page.begin(), page.end(), 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
            ph->page_size = static_cast<std::uint32_t>(page.size());
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            auto hh = ods::HeapPageHeader{};
            hh.num_slots = 0;
            hh.free_start = static_cast<std::uint16_t>(HeapLayout::tuples_region_start());
            hh.dir_start = static_cast<std::uint16_t>(page.size());
            hh.flags = HeapPageFlagBits::HasFreeSpace;
            write_heap_hdr(page, hh);
        }

        static ods::HeapPageHeader read_heap_hdr(const std::vector<std::uint8_t>& page)
        {
            ods::HeapPageHeader h{};
            std::memcpy(&h, page.data() + HeapLayout::page_header_size(),
                        sizeof(ods::HeapPageHeader));
            return h;
        }

        static void write_heap_hdr(std::vector<std::uint8_t>& page, const ods::HeapPageHeader& h)
        {
            std::memcpy(page.data() + HeapLayout::page_header_size(), &h,
                        sizeof(ods::HeapPageHeader));
        }

        static std::uint32_t free_bytes(const std::vector<std::uint8_t>& page)
        {
            auto h = read_heap_hdr(page);
            return static_cast<std::uint32_t>(h.dir_start) -
                   static_cast<std::uint32_t>(h.free_start);
        }

        // Write raw tuple bytes into the tuples region, return start offset. Caller ensures space.
        static std::uint16_t write_raw_tuple(std::vector<std::uint8_t>& page,
                                             const std::vector<std::uint8_t>& bytes)
        {
            auto h = read_heap_hdr(page);
            // 2-byte alignment by default for tuple payloads
            std::uint16_t at = static_cast<std::uint16_t>((h.free_start + 1u) & ~1u);
            if (static_cast<std::uint32_t>(at) + bytes.size() >
                static_cast<std::uint32_t>(h.dir_start))
                throw std::runtime_error("no space for tuple payload");
            // pad if needed
            if (at > h.free_start) {
                std::memset(page.data() + h.free_start, 0, at - h.free_start);
            }
            std::memcpy(page.data() + at, bytes.data(), bytes.size());
            h.free_start = static_cast<std::uint16_t>(at + bytes.size());
            write_heap_hdr(page, h);
            return at;
        }

        // Variant that allows specifying an explicit alignment (e.g., 8 for all-Int64 rows)
        static std::uint16_t write_raw_tuple_aligned(std::vector<std::uint8_t>& page,
                                                     const std::vector<std::uint8_t>& bytes,
                                                     std::uint16_t align)
        {
            if (align == 0)
                align = 1;
            auto h = read_heap_hdr(page);
            std::uint16_t at = static_cast<std::uint16_t>((h.free_start + (align - 1)) &
                                                          static_cast<std::uint16_t>(~(align - 1)));
            if (static_cast<std::uint32_t>(at) + bytes.size() >
                static_cast<std::uint32_t>(h.dir_start))
                throw std::runtime_error("no space for tuple payload (aligned)");
            if (at > h.free_start) {
                std::memset(page.data() + h.free_start, 0, at - h.free_start);
            }
            std::memcpy(page.data() + at, bytes.data(), bytes.size());
            h.free_start = static_cast<std::uint16_t>(at + bytes.size());
            write_heap_hdr(page, h);
            return at;
        }

        // Append a slot pointing to offset; returns the slot index (0-based).
        static std::uint16_t push_slot(std::vector<std::uint8_t>& page, std::uint16_t offset)
        {
            auto h = read_heap_hdr(page);
            if (h.dir_start < HeapLayout::tuples_region_start())
                throw std::runtime_error("invalid dir_start");
            if (h.free_start > h.dir_start)
                throw std::runtime_error("free_start past dir_start");
            // new base of directory after adding one 2-byte entry
            std::uint16_t new_dir =
                static_cast<std::uint16_t>(h.dir_start - ods::HEAP_SLOT_SIZE_BYTES);
            if (new_dir < h.free_start)
                throw std::runtime_error("no space for slot directory entry");
            // Write offset at new_dir
            std::memcpy(page.data() + new_dir, &offset, sizeof(offset));
            h.dir_start = new_dir;
            std::uint16_t slot_index = h.num_slots;
            h.num_slots = static_cast<std::uint16_t>(h.num_slots + 1);
            // Update flags
            h.flags = (free_bytes(page) > 0) ? HeapPageFlagBits::HasFreeSpace : 0;
            write_heap_hdr(page, h);
            return slot_index;
        }

        // Update an existing slot's offset
        static void set_slot_offset(std::vector<std::uint8_t>& page, std::uint16_t slot_index,
                                    std::uint16_t offset)
        {
            auto h = read_heap_hdr(page);
            if (slot_index >= h.num_slots)
                throw std::runtime_error("set_slot_offset: slot OOB");
            std::size_t pos = page.size() - (static_cast<std::size_t>(slot_index) + 1) *
                                                ods::HEAP_SLOT_SIZE_BYTES;
            std::memcpy(page.data() + pos, &offset, sizeof(offset));
        }

        static bool check_heap_page_invariants(const std::vector<std::uint8_t>& page,
                                               std::string& error)
        {
            error.clear();
            if (page.size() < HeapLayout::tuples_region_start()) {
                error = "page too small";
                return false;
            }
            auto* ph = reinterpret_cast<const ods::PageHeader*>(page.data());
            if (ph->page_size != page.size()) {
                error = "page_size mismatch";
                return false;
            }
            if (ph->type != static_cast<std::uint16_t>(ods::PageType::HeapData)) {
                error = "wrong type";
                return false;
            }
            auto h = read_heap_hdr(page);
            if (h.free_start < HeapLayout::tuples_region_start()) {
                error = "free_start before tuples region";
                return false;
            }
            if (h.dir_start > page.size()) {
                error = "dir_start beyond page";
                return false;
            }
            if (h.free_start > h.dir_start) {
                error = "free_start exceeds dir_start";
                return false;
            }
            // Validate each slot offset falls within [tuples_start, dir_start)
            for (int i = 0; i < h.num_slots; ++i) {
                std::uint16_t off = 0;
                std::memcpy(&off, page.data() + (page.size() - (i + 1) * ods::HEAP_SLOT_SIZE_BYTES),
                            2);
                if (off == 0)
                    continue; // dead/unused slot allowed
                if (off < HeapLayout::tuples_region_start() || off >= h.dir_start) {
                    error = "slot offset OOB";
                    return false;
                }
            }
            return true;
        }
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_HEAP_H
