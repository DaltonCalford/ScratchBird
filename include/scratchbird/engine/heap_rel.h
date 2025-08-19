#ifndef SCRATCHBIRD_ENGINE_HEAP_REL_H
#define SCRATCHBIRD_ENGINE_HEAP_REL_H

#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/txn.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    class HeapScan; // forward declaration

    struct InsertResult {
        ods::RowId rid{};
        std::uint32_t bytes_written{0};
        bool overflow{false};
    };

    class HeapRelation
    {
      public:
        HeapRelation(FileMap fmap, std::uint32_t page_size, std::uint32_t root_page,
                     TupleLayout layout)
            : fmap_(std::move(fmap)), page_size_(page_size), root_page_(root_page),
              layout_(std::move(layout))
        {
        }
        struct RelRoot {
            std::uint32_t root_page{0};
            std::uint32_t first_heap_page{0};
            std::uint32_t last_heap_page{0};
            std::uint32_t tuple_format_id{0};
        };

        static HeapRelation create(FileMap fmap, std::uint32_t page_size, const TupleLayout& layout,
                                   const HeapOptions& opts = {})
        {
            // Allocate pages via Allocator; for test isolation we may reserve up to a marker.
            Allocator alloc(&fmap, page_size);
            alloc.init_new();
            alloc.reserve_until(999);
            std::uint32_t root_page = alloc.allocate_free_page();
            std::uint32_t data_page = alloc.allocate_free_page();
            // Root page
            std::vector<std::uint8_t> root(page_size, 0);
            auto* ph = reinterpret_cast<ods::PageHeader*>(root.data());
            ph->page_size = page_size;
            ph->page_no = root_page;
            ph->type = static_cast<std::uint16_t>(ods::PageType::HeapRoot);
            ods::HeapRootPayload hr{};
            hr.version = 1;
            hr.first_heap_page = data_page;
            hr.last_heap_page = data_page;
            hr.tuple_format_id = compute_layout_format_id(layout);
            std::memcpy(root.data() + sizeof(ods::PageHeader), &hr, sizeof hr);
            // checksum root
            ph->checksum = 0;
            ph->checksum = ods::crc32c(root.data(), root.size());
            fmap.write_page(root_page, root.data());
            // First data page
            std::vector<std::uint8_t> page(page_size, 0);
            auto* ph2 = reinterpret_cast<ods::PageHeader*>(page.data());
            ph2->page_size = page_size;
            ph2->page_no = data_page;
            ph2->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(page);
            ph2->checksum = 0;
            ph2->checksum = ods::crc32c(page.data(), page.size());
            fmap.write_page(data_page, page.data());
            HeapRelation rel(std::move(fmap), page_size, root_page, layout);
            rel.opts_ = opts;
            return rel;
        }

        static HeapRelation open(FileMap fmap, std::uint32_t page_size, std::uint32_t root_page,
                                 const TupleLayout& layout)
        {
            return HeapRelation(std::move(fmap), page_size, root_page, layout);
        }

        HeapScan open_scan() const;
        HeapScan open_scan_visible(const SnapshotRC& snap,
                                   std::function<TxnState(std::uint64_t)> read_state) const;
        // RR variant: reuse same iterator, caller supplies cutoff via SnapshotRR but adapter passes
        // SnapshotRC-equivalent
        HeapScan open_scan_visible(const SnapshotRR& snap,
                                   std::function<TxnState(std::uint64_t)> read_state) const;

        InsertResult insert(const std::vector<Value>& values)
        {
            // Read root and prepare allocator
            ods::HeapRootPayload hr{};
            read_root(hr);
            Allocator alloc(&fmap_, page_size_);

            // Ensure we have a heap data page with enough space first
            std::vector<std::uint8_t> page(page_size_, 0);
            std::uint32_t data_pgno = hr.last_heap_page ? hr.last_heap_page : 0;
            if (data_pgno) {
                fmap_.read_page(data_pgno, page.data());
            }
            auto freeb = HeapPageCodec::free_bytes(page);
            std::uint32_t threshold =
                opts_.free_space_threshold_bytes ? opts_.free_space_threshold_bytes : 64;
            if (data_pgno == 0 || freeb < threshold) {
                std::uint32_t newp = alloc.allocate_free_page();
                std::vector<std::uint8_t> np(page_size_, 0);
                auto* nph = reinterpret_cast<ods::PageHeader*>(np.data());
                nph->page_size = page_size_;
                nph->page_no = newp;
                nph->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
                HeapPageCodec::init_heap_data_page(np);
                nph->checksum = 0;
                nph->checksum = ods::crc32c(np.data(), np.size());
                fmap_.write_page(newp, np.data());
                data_pgno = newp;
                fmap_.read_page(data_pgno, page.data());
                if (hr.first_heap_page == 0)
                    hr.first_heap_page = data_pgno;
                if (data_pgno > hr.last_heap_page)
                    hr.last_heap_page = data_pgno;
            }

            // Debug: header before write
            {
                auto hh = HeapPageCodec::read_heap_hdr(page);
                std::fprintf(stderr,
                             "[HEAP INSERT] before write: root=%u target=%u slots=%u free_start=%u "
                             "dir_start=%u threshold=%u freeb=%u\n",
                             root_page_, data_pgno, (unsigned)hh.num_slots, (unsigned)hh.free_start,
                             (unsigned)hh.dir_start, (unsigned)threshold, (unsigned)freeb);
            }

            // Overflow writer using allocator
            bool wrote_overflow = false;
            auto write_overflow = [&](const std::uint8_t* data, std::size_t len) -> std::uint32_t {
                std::uint32_t first = 0;
                std::size_t remaining = len;
                const std::uint8_t* ptr = data;
                std::optional<std::uint32_t> reserved_next;
                while (remaining > 0) {
                    std::uint32_t pgno = 0;
                    if (reserved_next.has_value()) {
                        pgno = *reserved_next;
                        reserved_next.reset();
                    } else {
                        pgno = alloc.allocate_free_page();
                    }
                    if (!first)
                        first = pgno;
                    std::size_t max_chunk = page_size_ - sizeof(ods::PageHeader) - 8;
                    std::uint32_t chunk =
                        static_cast<std::uint32_t>(std::min<std::size_t>(remaining, max_chunk));
                    bool have_more = remaining > chunk;
                    std::uint32_t next_pgno = 0;
                    if (have_more) {
                        next_pgno = alloc.allocate_free_page();
                        reserved_next = next_pgno;
                    }
                    std::vector<std::uint8_t> op(page_size_, 0);
                    auto* ph = reinterpret_cast<ods::PageHeader*>(op.data());
                    ph->page_size = page_size_;
                    ph->page_no = pgno;
                    ph->type = static_cast<std::uint16_t>(ods::PageType::HeapOverflow);
                    std::size_t p = sizeof(ods::PageHeader);
                    std::memcpy(op.data() + p, &chunk, 4);
                    p += 4;
                    std::memcpy(op.data() + p, &next_pgno, 4);
                    std::memcpy(op.data() + p + 4, ptr, chunk);
                    auto* oph = reinterpret_cast<ods::PageHeader*>(op.data());
                    oph->checksum = 0;
                    oph->checksum = ods::crc32c(op.data(), op.size());
                    fmap_.write_page(pgno, op.data());
                    remaining -= chunk;
                    ptr += chunk;
                    wrote_overflow = true;
                    if (pgno > hr.last_heap_page)
                        hr.last_heap_page = pgno;
                }
                return first;
            };

            std::vector<std::uint8_t> bytes = HeapTupleCodec::encode_tuple_with_overflow(
                layout_, values, page_size_, write_overflow, opts_.overflow_threshold_pct);
            std::uint32_t need =
                static_cast<std::uint32_t>(bytes.size() + ods::HEAP_SLOT_SIZE_BYTES);
            if (HeapPageCodec::free_bytes(page) < need) {
                std::uint32_t newp = alloc.allocate_free_page();
                std::vector<std::uint8_t> np(page_size_, 0);
                auto* nph = reinterpret_cast<ods::PageHeader*>(np.data());
                nph->page_size = page_size_;
                nph->page_no = newp;
                nph->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
                HeapPageCodec::init_heap_data_page(np);
                nph->checksum = 0;
                nph->checksum = ods::crc32c(np.data(), np.size());
                fmap_.write_page(newp, np.data());
                data_pgno = newp;
                fmap_.read_page(data_pgno, page.data());
                if (data_pgno > hr.last_heap_page)
                    hr.last_heap_page = data_pgno;
            }

            // Write tuple and slot
            bool all_eight = true;
            for (const auto& a : layout_.attrs) {
                if (!(a.type == AttrType::Int64 && a.fixed_len == 8 && a.by_val)) {
                    all_eight = false;
                    break;
                }
            }
            std::uint16_t off = all_eight ? HeapPageCodec::write_raw_tuple_aligned(page, bytes, 8)
                                          : HeapPageCodec::write_raw_tuple(page, bytes);
            std::uint16_t slot = HeapPageCodec::push_slot(page, off);
            {
                auto* dph = reinterpret_cast<ods::PageHeader*>(page.data());
                dph->checksum = 0;
                dph->checksum = ods::crc32c(page.data(), page.size());
            }
            fmap_.write_page(data_pgno, page.data());
            write_root(hr);
            // Debug: header after write
            {
                std::vector<std::uint8_t> ver(page_size_, 0);
                fmap_.read_page(data_pgno, ver.data());
                auto hh = HeapPageCodec::read_heap_hdr(ver);
                std::fprintf(stderr,
                             "[HEAP INSERT] after write: root=%u target=%u slots=%u free_start=%u "
                             "dir_start=%u\n",
                             root_page_, data_pgno, (unsigned)hh.num_slots, (unsigned)hh.free_start,
                             (unsigned)hh.dir_start);
            }
            InsertResult res{};
            res.bytes_written = static_cast<std::uint32_t>(bytes.size());
            res.rid = ods::RowId{1, data_pgno, static_cast<std::uint16_t>(slot)};
            res.overflow = wrote_overflow;
            return res;
        }

        // Phase 3: insert with transaction (set created_xid in tuple header before write)
        InsertResult insert_txn(const std::vector<Value>& values, const Transaction& tx)
        {
            // Read root and prepare allocator
            ods::HeapRootPayload hr{};
            read_root(hr);
            Allocator alloc(&fmap_, page_size_);

            // Ensure we have a heap data page with enough space first
            std::vector<std::uint8_t> page(page_size_, 0);
            std::uint32_t data_pgno = hr.last_heap_page ? hr.last_heap_page : 0;
            if (data_pgno) {
                fmap_.read_page(data_pgno, page.data());
            }
            auto freeb = HeapPageCodec::free_bytes(page);
            std::uint32_t threshold =
                opts_.free_space_threshold_bytes ? opts_.free_space_threshold_bytes : 64;
            if (data_pgno == 0 || freeb < threshold) {
                std::uint32_t newp = alloc.allocate_free_page();
                std::vector<std::uint8_t> np(page_size_, 0);
                auto* nph = reinterpret_cast<ods::PageHeader*>(np.data());
                nph->page_size = page_size_;
                nph->page_no = newp;
                nph->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
                HeapPageCodec::init_heap_data_page(np);
                nph->checksum = 0;
                nph->checksum = ods::crc32c(np.data(), np.size());
                fmap_.write_page(newp, np.data());
                data_pgno = newp;
                fmap_.read_page(data_pgno, page.data());
                if (hr.first_heap_page == 0)
                    hr.first_heap_page = data_pgno;
                if (data_pgno > hr.last_heap_page)
                    hr.last_heap_page = data_pgno;
            }

            // Overflow writer using allocator
            bool wrote_overflow = false;
            auto write_overflow = [&](const std::uint8_t* data, std::size_t len) -> std::uint32_t {
                std::uint32_t first = 0;
                std::size_t remaining = len;
                const std::uint8_t* ptr = data;
                std::optional<std::uint32_t> reserved_next;
                while (remaining > 0) {
                    std::uint32_t pgno = 0;
                    if (reserved_next.has_value()) {
                        pgno = *reserved_next;
                        reserved_next.reset();
                    } else {
                        pgno = alloc.allocate_free_page();
                    }
                    if (!first)
                        first = pgno;
                    std::size_t max_chunk = page_size_ - sizeof(ods::PageHeader) - 8;
                    std::uint32_t chunk =
                        static_cast<std::uint32_t>(std::min<std::size_t>(remaining, max_chunk));
                    bool have_more = remaining > chunk;
                    std::uint32_t next_pgno = 0;
                    if (have_more) {
                        next_pgno = alloc.allocate_free_page();
                        reserved_next = next_pgno;
                    }
                    std::vector<std::uint8_t> op(page_size_, 0);
                    auto* ph = reinterpret_cast<ods::PageHeader*>(op.data());
                    ph->page_size = page_size_;
                    ph->page_no = pgno;
                    ph->type = static_cast<std::uint16_t>(ods::PageType::HeapOverflow);
                    std::size_t p = sizeof(ods::PageHeader);
                    std::memcpy(op.data() + p, &chunk, 4);
                    p += 4;
                    std::memcpy(op.data() + p, &next_pgno, 4);
                    std::memcpy(op.data() + p + 4, ptr, chunk);
                    auto* oph = reinterpret_cast<ods::PageHeader*>(op.data());
                    oph->checksum = 0;
                    oph->checksum = ods::crc32c(op.data(), op.size());
                    fmap_.write_page(pgno, op.data());
                    remaining -= chunk;
                    ptr += chunk;
                    wrote_overflow = true;
                    if (pgno > hr.last_heap_page)
                        hr.last_heap_page = pgno;
                }
                return first;
            };

            std::vector<std::uint8_t> bytes = HeapTupleCodec::encode_tuple_with_overflow(
                layout_, values, page_size_, write_overflow, opts_.overflow_threshold_pct, 128,
                tx.id);

            std::uint32_t need =
                static_cast<std::uint32_t>(bytes.size() + ods::HEAP_SLOT_SIZE_BYTES);
            if (HeapPageCodec::free_bytes(page) < need) {
                std::uint32_t newp = alloc.allocate_free_page();
                std::vector<std::uint8_t> np(page_size_, 0);
                auto* nph = reinterpret_cast<ods::PageHeader*>(np.data());
                nph->page_size = page_size_;
                nph->page_no = newp;
                nph->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
                HeapPageCodec::init_heap_data_page(np);
                nph->checksum = 0;
                nph->checksum = ods::crc32c(np.data(), np.size());
                fmap_.write_page(newp, np.data());
                data_pgno = newp;
                fmap_.read_page(data_pgno, page.data());
                if (data_pgno > hr.last_heap_page)
                    hr.last_heap_page = data_pgno;
            }

            // Write tuple and slot
            bool all_eight = true;
            for (const auto& a : layout_.attrs) {
                if (!(a.type == AttrType::Int64 && a.fixed_len == 8 && a.by_val)) {
                    all_eight = false;
                    break;
                }
            }
            std::uint16_t off = all_eight ? HeapPageCodec::write_raw_tuple_aligned(page, bytes, 8)
                                          : HeapPageCodec::write_raw_tuple(page, bytes);
            std::uint16_t slot = HeapPageCodec::push_slot(page, off);
            {
                auto* dph = reinterpret_cast<ods::PageHeader*>(page.data());
                dph->checksum = 0;
                dph->checksum = ods::crc32c(page.data(), page.size());
            }
            fmap_.write_page(data_pgno, page.data());
            write_root(hr);
            // debug: insert written (disabled in CI)
            InsertResult res{};
            res.bytes_written = static_cast<std::uint32_t>(bytes.size());
            res.rid = ods::RowId{1, data_pgno, static_cast<std::uint16_t>(slot)};
            res.overflow = wrote_overflow;
            return res;
        }

        // Truncate: free all data/overflow pages, keep HeapRoot and allocate a fresh data page
        void truncate()
        {
            Allocator alloc(&fmap_, page_size_);
            ods::HeapRootPayload hr{};
            read_root(hr);
            if (hr.first_heap_page && hr.last_heap_page &&
                hr.first_heap_page <= hr.last_heap_page) {
                for (std::uint32_t p = hr.first_heap_page; p <= hr.last_heap_page; ++p) {
                    std::vector<std::uint8_t> buf(page_size_, 0);
                    fmap_.read_page(p, buf.data());
                    auto* ph = reinterpret_cast<ods::PageHeader*>(buf.data());
                    if (ph->type == static_cast<std::uint16_t>(ods::PageType::HeapData) ||
                        ph->type == static_cast<std::uint16_t>(ods::PageType::HeapOverflow)) {
                        alloc.free_page(p);
                    }
                }
            }
            // allocate a new empty data page
            std::uint32_t new_data = alloc.allocate_free_page();
            std::vector<std::uint8_t> np(page_size_, 0);
            auto* nph = reinterpret_cast<ods::PageHeader*>(np.data());
            nph->page_size = page_size_;
            nph->page_no = new_data;
            nph->type = static_cast<std::uint16_t>(ods::PageType::HeapData);
            HeapPageCodec::init_heap_data_page(np);
            nph->checksum = 0;
            nph->checksum = ods::crc32c(np.data(), np.size());
            fmap_.write_page(new_data, np.data());
            hr.first_heap_page = new_data;
            hr.last_heap_page = new_data;
            write_root(hr);
        }

        // Drop: free all data/overflow pages and the HeapRoot page itself
        void drop()
        {
            Allocator alloc(&fmap_, page_size_);
            ods::HeapRootPayload hr{};
            read_root(hr);
            if (hr.first_heap_page && hr.last_heap_page &&
                hr.first_heap_page <= hr.last_heap_page) {
                for (std::uint32_t p = hr.first_heap_page; p <= hr.last_heap_page; ++p) {
                    std::vector<std::uint8_t> buf(page_size_, 0);
                    fmap_.read_page(p, buf.data());
                    auto* ph = reinterpret_cast<ods::PageHeader*>(buf.data());
                    if (ph->type == static_cast<std::uint16_t>(ods::PageType::HeapData) ||
                        ph->type == static_cast<std::uint16_t>(ods::PageType::HeapOverflow)) {
                        alloc.free_page(p);
                    }
                }
            }
            // Finally free root page
            alloc.free_page(root_page_);
            // Invalidate local cache
            root_page_ = 0;
        }

        bool fetch(const ods::RowId& rid, std::vector<Value>& out) const
        {
            std::vector<std::uint8_t> page(page_size_, 0);
            fmap_.read_page(rid.page_no, page.data());
            // read slot offset
            auto hh = HeapPageCodec::read_heap_hdr(page);
            if (rid.slot_no >= hh.num_slots)
                return false;
            std::uint16_t off = 0;
            std::memcpy(&off,
                        page.data() + (page.size() - (rid.slot_no + 1) * ods::HEAP_SLOT_SIZE_BYTES),
                        2);
            if (off == 0)
                return false;
            auto read_overflow = [&](const ods::OverflowRef& ref, std::string& out_bytes) -> bool {
                out_bytes.clear();
                std::uint32_t remaining = ref.length;
                std::uint32_t pg = ref.page_no;
                while (remaining > 0 && pg != 0) {
                    std::vector<std::uint8_t> op(page_size_, 0);
                    fmap_.read_page(pg, op.data());
                    if (op.size() < sizeof(ods::PageHeader) + 8)
                        return false;
                    std::uint32_t l32 = 0;
                    std::memcpy(&l32, op.data() + sizeof(ods::PageHeader), 4);
                    std::uint32_t nextpg = 0;
                    std::memcpy(&nextpg, op.data() + sizeof(ods::PageHeader) + 4, 4);
                    std::size_t data_off = sizeof(ods::PageHeader) + 8;
                    if (data_off + l32 > op.size())
                        return false;
                    std::size_t take = std::min<std::uint32_t>(l32, remaining);
                    out_bytes.append(reinterpret_cast<const char*>(op.data() + data_off), take);
                    remaining -= take;
                    pg = nextpg;
                }
                return remaining == 0;
            };
            // Phase 3: head-only decode here; scans will add full visibility filtering
            return HeapTupleCodec::decode_tuple(layout_, page, off, out, read_overflow);
        }

        bool fetch_visible(const ods::RowId& rid, std::vector<Value>& out,
                           const SnapshotRC& /*snap*/,
                           const std::function<TxnState(std::uint64_t)>& /*read_state*/) const
        {
            // For now, reuse fetch; visibility filtering is provided by scans
            bool ok = fetch(rid, out);
            return ok;
        }

        // Stubs for future phases
        // Non-transactional update (not used in MGA; kept for API completeness)
        bool update(const ods::RowId& rid, const std::vector<Value>& values, ods::RowId* new_rid)
        {
            // Do copy-on-write: insert new tuple version, point slot to new one, mark backptr
            Transaction fake{};
            fake.id = 0;
            auto ins = insert_txn(values, fake);
            // Read old tuple header to set backptr on new version if possible
            std::vector<std::uint8_t> old_page(page_size_, 0);
            fmap_.read_page(rid.page_no, old_page.data());
            std::uint16_t off = 0;
            std::memcpy(&off,
                        old_page.data() +
                            (old_page.size() - (rid.slot_no + 1) * ods::HEAP_SLOT_SIZE_BYTES),
                        2);
            if (off != 0 && off + sizeof(ods::TupleHeader) <= old_page.size()) {
                ods::TupleHeader old_th{};
                std::memcpy(&old_th, old_page.data() + off, sizeof old_th);
                // Patch new version's backptr
                std::vector<std::uint8_t> new_page(page_size_, 0);
                fmap_.read_page(ins.rid.page_no, new_page.data());
                std::uint16_t new_off = 0;
                std::memcpy(&new_off,
                            new_page.data() + (new_page.size() -
                                               (ins.rid.slot_no + 1) * ods::HEAP_SLOT_SIZE_BYTES),
                            2);
                if (new_off != 0 && new_off + sizeof(ods::TupleHeader) <= new_page.size()) {
                    ods::TupleHeader new_th{};
                    std::memcpy(&new_th, new_page.data() + new_off, sizeof new_th);
                    new_th.backptr_rid = ods::pack_rowid(rid);
                    std::memcpy(new_page.data() + new_off, &new_th, sizeof new_th);
                    auto* dph = reinterpret_cast<ods::PageHeader*>(new_page.data());
                    dph->checksum = 0;
                    dph->checksum = ods::crc32c(new_page.data(), new_page.size());
                    fmap_.write_page(ins.rid.page_no, new_page.data());
                }
            }
            if (new_rid)
                *new_rid = ins.rid;
            // Point original slot to new tuple (move head)
            std::vector<std::uint8_t> page(page_size_, 0);
            fmap_.read_page(ins.rid.page_no, page.data());
            HeapPageCodec::set_slot_offset(page, ins.rid.slot_no,
                                           static_cast<std::uint16_t>(ins.rid.slot_no));
            // leave original slot as is; non-txn API does not mark deletes
            return true;
        }
        bool remove(const ods::RowId& rid)
        {
            // Non-transactional hard remove not supported; mark slot dead
            std::vector<std::uint8_t> page(page_size_, 0);
            fmap_.read_page(rid.page_no, page.data());
            auto h = HeapPageCodec::read_heap_hdr(page);
            if (rid.slot_no >= h.num_slots)
                return false;
            std::uint16_t zero = 0;
            std::size_t pos = page.size() - (static_cast<std::size_t>(rid.slot_no) + 1) *
                                                ods::HEAP_SLOT_SIZE_BYTES;
            std::memcpy(page.data() + pos, &zero, 2);
            auto* dph = reinterpret_cast<ods::PageHeader*>(page.data());
            dph->checksum = 0;
            dph->checksum = ods::crc32c(page.data(), page.size());
            fmap_.write_page(rid.page_no, page.data());
            return true;
        }

        // Transactional update with version chain and delete-mark on old
        bool update_txn(const ods::RowId& old_rid, const std::vector<Value>& values,
                        const Transaction& tx, ods::RowId* new_rid)
        {
            // Acquire write lock on old_rid to prevent WW conflicts
            if (!LockManager::acquire_write_lock(old_rid, tx.id))
                return false;
            // Insert new version with created_xid=tx.id
            auto ins = insert_txn(values, tx);
            // Set backptr on new version to old_rid
            std::vector<std::uint8_t> new_page(page_size_, 0);
            fmap_.read_page(ins.rid.page_no, new_page.data());
            std::uint16_t new_off = 0;
            std::memcpy(&new_off,
                        new_page.data() +
                            (new_page.size() - (ins.rid.slot_no + 1) * ods::HEAP_SLOT_SIZE_BYTES),
                        2);
            if (new_off == 0 || new_off + sizeof(ods::TupleHeader) > new_page.size())
                return false;
            ods::TupleHeader new_th{};
            std::memcpy(&new_th, new_page.data() + new_off, sizeof new_th);
            new_th.backptr_rid = ods::pack_rowid(old_rid);
            std::memcpy(new_page.data() + new_off, &new_th, sizeof new_th);
            {
                auto* dph = reinterpret_cast<ods::PageHeader*>(new_page.data());
                dph->checksum = 0;
                dph->checksum = ods::crc32c(new_page.data(), new_page.size());
            }
            fmap_.write_page(ins.rid.page_no, new_page.data());
            // Mark old as deleted by tx
            if (!remove_txn(old_rid, tx))
                return false;
            if (new_rid)
                *new_rid = ins.rid;
            LockManager::release_write_lock(old_rid, tx.id);
            return true;
        }

        // Phase 3: transactional delete (mark deleted_xid)
        bool remove_txn(const ods::RowId& rid, const Transaction& tx)
        {
            std::vector<std::uint8_t> page(page_size_, 0);
            fmap_.read_page(rid.page_no, page.data());
            auto hh = HeapPageCodec::read_heap_hdr(page);
            if (rid.slot_no >= hh.num_slots)
                return false;
            std::uint16_t off = 0;
            std::memcpy(&off,
                        page.data() + (page.size() - (rid.slot_no + 1) * ods::HEAP_SLOT_SIZE_BYTES),
                        2);
            if (off == 0)
                return false;
            if (off + sizeof(ods::TupleHeader) > page.size())
                return false;
            ods::TupleHeader th{};
            std::memcpy(&th, page.data() + off, sizeof th);
            if (th.deleted_xid != 0)
                return false; // already deleted
            th.deleted_xid = tx.id;
            std::memcpy(page.data() + off, &th, sizeof th);
            auto* dph = reinterpret_cast<ods::PageHeader*>(page.data());
            dph->checksum = 0;
            dph->checksum = ods::crc32c(page.data(), page.size());
            fmap_.write_page(rid.page_no, page.data());
            return true;
        }

      private:
        // Backward probe for a page with enough free space (simple scan Phase 1)
        std::optional<std::uint32_t> find_page_with_freespace(std::uint32_t start,
                                                              std::uint32_t need) const
        {
            if (start == 0)
                return std::nullopt;
            for (std::int64_t p = static_cast<std::int64_t>(start); p >= 1000; --p) {
                std::vector<std::uint8_t> buf(page_size_, 0);
                fmap_.read_page(static_cast<std::uint32_t>(p), buf.data());
                auto* ph = reinterpret_cast<ods::PageHeader*>(buf.data());
                if (ph->type != static_cast<std::uint16_t>(ods::PageType::HeapData))
                    continue;
                if (HeapPageCodec::free_bytes(buf) >= need)
                    return static_cast<std::uint32_t>(p);
            }
            return std::nullopt;
        }
        void read_root(ods::HeapRootPayload& out)
        {
            std::vector<std::uint8_t> root(page_size_, 0);
            fmap_.read_page(root_page_, root.data());
            std::memcpy(&out, root.data() + sizeof(ods::PageHeader), sizeof out);
        }
        void write_root(const ods::HeapRootPayload& in)
        {
            std::vector<std::uint8_t> root(page_size_, 0);
            fmap_.read_page(root_page_, root.data());
            std::memcpy(root.data() + sizeof(ods::PageHeader), &in, sizeof in);
            // Update checksum on root page
            auto* rph = reinterpret_cast<ods::PageHeader*>(root.data());
            rph->checksum = 0;
            rph->checksum = ods::crc32c(root.data(), root.size());
            fmap_.write_page(root_page_, root.data());
        }

        FileMap fmap_;
        std::uint32_t page_size_{4096};
        std::uint32_t root_page_{0};
        TupleLayout layout_{};
        RelRoot rel_cache_{};
        HeapOptions opts_{};
    };

    class HeapScan
    {
      public:
        HeapScan(FileMap* fmap, std::uint32_t page_size, std::uint32_t first_page,
                 std::uint32_t last_page, const TupleLayout* layout)
            : fmap_(fmap), page_size_(page_size), cur_page_no_(first_page), last_page_(last_page),
              layout_(layout)
        {
            page_.resize(page_size_);
            load_page();
        }
        HeapScan(FileMap* fmap, std::uint32_t page_size, std::uint32_t first_page,
                 std::uint32_t last_page, const TupleLayout* layout, const SnapshotRC& snap,
                 std::function<TxnState(std::uint64_t)> read_state)
            : fmap_(fmap), page_size_(page_size), cur_page_no_(first_page), last_page_(last_page),
              layout_(layout), vis_enabled_(true), rr_mode_(false), snap_(snap),
              read_state_(std::move(read_state))
        {
            page_.resize(page_size_);
            load_page();
        }
        HeapScan(FileMap* fmap, std::uint32_t page_size, std::uint32_t first_page,
                 std::uint32_t last_page, const TupleLayout* layout, const SnapshotRR& snap,
                 std::function<TxnState(std::uint64_t)> read_state)
            : fmap_(fmap), page_size_(page_size), cur_page_no_(first_page), last_page_(last_page),
              layout_(layout), vis_enabled_(true), rr_mode_(true), rr_snap_(snap),
              read_state_(std::move(read_state))
        {
            page_.resize(page_size_);
            load_page();
        }
        bool next(std::vector<Value>& out, ods::RowId* rid_out)
        {
            while (true) {
                if (cur_page_no_ > last_page_)
                    return false;
                std::uint16_t slots_by_dir = static_cast<std::uint16_t>(
                    (page_.size() - header_.dir_start) / ods::HEAP_SLOT_SIZE_BYTES);
                std::uint16_t slots = header_.num_slots ? header_.num_slots : slots_by_dir;
                if (slot_index_ >= slots) {
                    ++cur_page_no_;
                    slot_index_ = 0;
                    load_page();
                    continue;
                }
                // read slot
                std::uint16_t off = 0;
                std::memcpy(&off,
                            page_.data() +
                                (page_.size() - (slot_index_ + 1) * ods::HEAP_SLOT_SIZE_BYTES),
                            2);
                ++slot_index_;
                if (off == 0)
                    continue; // dead slot
                auto read_overflow = [&](const ods::OverflowRef& ref, std::string& out_bytes) {
                    std::uint32_t remaining = ref.length;
                    std::uint32_t pg = ref.page_no;
                    out_bytes.clear();
                    while (remaining > 0 && pg != 0) {
                        std::vector<std::uint8_t> op(page_size_, 0);
                        fmap_->read_page(pg, op.data());
                        std::uint32_t l32 = 0;
                        std::memcpy(&l32, op.data() + sizeof(ods::PageHeader), 4);
                        std::uint32_t nextpg = 0;
                        std::memcpy(&nextpg, op.data() + sizeof(ods::PageHeader) + 4, 4);
                        std::size_t data_off = sizeof(ods::PageHeader) + 8;
                        std::size_t take = std::min<std::uint32_t>(l32, remaining);
                        out_bytes.append(reinterpret_cast<const char*>(op.data() + data_off), take);
                        remaining -= take;
                        pg = nextpg;
                    }
                    return remaining == 0;
                };
                // Visibility filtering if enabled
                if (vis_enabled_) {
                    if (off + sizeof(ods::TupleHeader) > page_.size())
                        continue;
                    ods::TupleHeader th{};
                    std::memcpy(&th, page_.data() + off, sizeof th);
                    if (rr_mode_) {
                        if (!is_visible_rr(rr_snap_, th, read_state_))
                            continue;
                    } else {
                        if (!is_visible_rc(snap_, th, read_state_))
                            continue;
                    }
                }
                if (!HeapTupleCodec::decode_tuple(*layout_, page_, off, out, read_overflow))
                    continue;
                if (rid_out)
                    *rid_out =
                        ods::RowId{1, cur_page_no_, static_cast<std::uint16_t>(slot_index_ - 1)};
                return true;
            }
        }

      private:
        void load_page()
        {
            if (cur_page_no_ <= last_page_) {
                // Optional prefetch current and next page with small horizon
                auto mapping = fmap_->map(cur_page_no_);
                FileManager::prefetch_willneed(fmap_->segments()[mapping.first].handle,
                                               mapping.second, page_size_);
                if (cur_page_no_ + 1 <= last_page_) {
                    auto mapping2 = fmap_->map(cur_page_no_ + 1);
                    FileManager::prefetch_willneed(fmap_->segments()[mapping2.first].handle,
                                                   mapping2.second, page_size_);
                }
                fmap_->read_page(cur_page_no_, page_.data());
                header_ = HeapPageCodec::read_heap_hdr(page_);
            }
        }

        FileMap* fmap_{};
        std::uint32_t page_size_{0};
        std::uint32_t cur_page_no_{0};
        std::uint32_t last_page_{0};
        const TupleLayout* layout_{};
        std::vector<std::uint8_t> page_{};
        ods::HeapPageHeader header_{};
        std::uint16_t slot_index_{0};
        bool vis_enabled_{false};
        bool rr_mode_{false};
        SnapshotRC snap_{};
        SnapshotRR rr_snap_{};
        std::function<TxnState(std::uint64_t)> read_state_{};
    };

    inline HeapScan HeapRelation::open_scan() const
    {
        ods::HeapRootPayload hr{};
        // read_root is non-const; cast for internal meta read
        const_cast<HeapRelation*>(this)->read_root(hr);
        return HeapScan(const_cast<FileMap*>(&fmap_), page_size_, hr.first_heap_page,
                        hr.last_heap_page, &layout_);
    }

    inline HeapScan
    HeapRelation::open_scan_visible(const SnapshotRC& snap,
                                    std::function<TxnState(std::uint64_t)> read_state) const
    {
        ods::HeapRootPayload hr{};
        const_cast<HeapRelation*>(this)->read_root(hr);
        return HeapScan(const_cast<FileMap*>(&fmap_), page_size_, hr.first_heap_page,
                        hr.last_heap_page, &layout_, snap, std::move(read_state));
    }

    inline HeapScan
    HeapRelation::open_scan_visible(const SnapshotRR& snap_rr,
                                    std::function<TxnState(std::uint64_t)> read_state) const
    {
        ods::HeapRootPayload hr{};
        const_cast<HeapRelation*>(this)->read_root(hr);
        return HeapScan(const_cast<FileMap*>(&fmap_), page_size_, hr.first_heap_page,
                        hr.last_heap_page, &layout_, snap_rr, std::move(read_state));
    }

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_HEAP_REL_H
