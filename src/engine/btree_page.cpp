#include "scratchbird/engine/btree_page.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace scratchbird::engine
{

    static void write16(std::vector<std::uint8_t>& p, std::size_t off, std::uint16_t v)
    {
        std::memcpy(p.data() + off, &v, 2);
    }
    static void write32(std::vector<std::uint8_t>& p, std::size_t off, std::uint32_t v)
    {
        std::memcpy(p.data() + off, &v, 4);
    }

    void detail::encode_key(const CompositeKey& key, std::vector<std::uint8_t>& out)
    {
        std::uint16_t np = static_cast<std::uint16_t>(key.parts.size());
        out.insert(out.end(), reinterpret_cast<std::uint8_t*>(&np),
                   reinterpret_cast<std::uint8_t*>(&np) + 2);
        for (const auto& part : key.parts) {
            std::uint8_t flags = (part.is_null ? 0x01 : 0x00) | (part.desc ? 0x02 : 0x00);
            out.push_back(flags);
            std::uint16_t len = static_cast<std::uint16_t>(part.bytes.size());
            out.insert(out.end(), reinterpret_cast<std::uint8_t*>(&len),
                       reinterpret_cast<std::uint8_t*>(&len) + 2);
            if (len)
                out.insert(out.end(), reinterpret_cast<const std::uint8_t*>(part.bytes.data()),
                           reinterpret_cast<const std::uint8_t*>(part.bytes.data()) + len);
        }
    }

    void detail::decode_key(const std::uint8_t* data, std::size_t len, CompositeKey& out)
    {
        out.parts.clear();
        if (len < 2)
            return;
        std::uint16_t np = 0;
        std::memcpy(&np, data, 2);
        std::size_t off = 2;
        out.parts.reserve(np);
        for (int i = 0; i < np && off + 3 <= len; ++i) {
            std::uint8_t flags = data[off++];
            std::uint16_t pl = 0;
            std::memcpy(&pl, data + off, 2);
            off += 2;
            KeyPart part{};
            part.is_null = (flags & 0x01) != 0;
            part.desc = (flags & 0x02) != 0;
            if (off + pl > len)
                pl = static_cast<std::uint16_t>(len - off);
            part.bytes.assign(reinterpret_cast<const char*>(data + off), pl);
            off += pl;
            out.parts.push_back(std::move(part));
        }
    }

    static void set_hdr(ods::PageHeader& hdr, std::uint32_t page_no, std::uint32_t page_size,
                        ods::PageType t)
    {
        hdr.page_no = page_no;
        hdr.page_size = page_size;
        hdr.type = static_cast<std::uint16_t>(t);
    }

    static void write_btree_hdr(std::vector<std::uint8_t>& page, const BTreeHdrV1& h)
    {
        std::memcpy(page.data() + sizeof(ods::PageHeader), &h, sizeof(BTreeHdrV1));
    }

    static BTreeHdrV1 read_btree_hdr(const std::vector<std::uint8_t>& page)
    {
        BTreeHdrV1 h{};
        std::memcpy(&h, page.data() + sizeof(ods::PageHeader), sizeof(BTreeHdrV1));
        return h;
    }

    static void write_slot(std::vector<std::uint8_t>& page, std::uint16_t base, int idx,
                           std::uint16_t off)
    {
        std::memcpy(page.data() + base + 2 * idx, &off, 2);
    }

    static std::uint16_t read_slot(const std::vector<std::uint8_t>& page, std::uint16_t base,
                                   int idx)
    {
        std::uint16_t off = 0;
        std::memcpy(&off, page.data() + base + 2 * idx, 2);
        return off;
    }

    static void encode_key_to(std::vector<std::uint8_t>& page, std::uint16_t& at,
                              const CompositeKey& k)
    {
        std::vector<std::uint8_t> tmp;
        tmp.reserve(2 + k.parts.size() * 8);
        detail::encode_key(k, tmp);
        std::uint16_t kl = static_cast<std::uint16_t>(tmp.size());
        write16(page, at, kl);
        at += 2;
        if (kl) {
            std::memcpy(page.data() + at, tmp.data(), kl);
            at += kl;
        }
    }

    static void decode_key_from(const std::vector<std::uint8_t>& page, std::uint16_t& off,
                                CompositeKey& k)
    {
        std::uint16_t kl = 0;
        std::memcpy(&kl, page.data() + off, 2);
        off += 2;
        detail::decode_key(page.data() + off, kl, k);
        off += kl;
    }

    void build_leaf_page_v1(std::vector<std::uint8_t>& page, std::uint32_t page_size,
                            const std::vector<LeafRecordV1>& records, const CompositeKey* high_key,
                            const std::string* per_page_prefix)
    {
        page.assign(page_size, 0);
        auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
        set_hdr(*ph, ph->page_no, page_size, ods::PageType::IndexLeaf);
        BTreeHdrV1 h{};
        h.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(BTreeHdrV1));
        h.dir_start = static_cast<std::uint16_t>(page_size);
        if (per_page_prefix && !per_page_prefix->empty()) {
            h.prefix_off = h.free_start;
            std::uint16_t pl = static_cast<std::uint16_t>(per_page_prefix->size());
            write16(page, h.free_start, pl);
            h.free_start += 2;
            if (pl) {
                std::memcpy(page.data() + h.free_start, per_page_prefix->data(), pl);
                h.free_start += pl;
            }
            h.flags |= static_cast<std::uint16_t>(BTreePageFlags::LeafPerPagePrefix);
        }
        if (high_key) {
            h.high_key_off = h.free_start;
            encode_key_to(page, h.free_start, *high_key);
        }
        // write records and slots
        std::vector<std::uint16_t> offs;
        offs.reserve(records.size());
        for (const auto& r : records) {
            offs.push_back(h.free_start);
            encode_key_to(page, h.free_start, r.key);
            // row_id
            page.resize(page_size); // ensure capacity not shrinking
            std::memcpy(page.data() + h.free_start, &r.row_id, 8);
            h.free_start += 8;
            // payload
            std::uint16_t pl = static_cast<std::uint16_t>(r.payload.size());
            write16(page, h.free_start, pl);
            h.free_start += 2;
            if (pl) {
                std::memcpy(page.data() + h.free_start, r.payload.data(), pl);
                h.free_start += pl;
            }
            if (h.free_start + 2 > h.dir_start)
                throw std::runtime_error("leaf overflow");
            h.num_slots++;
        }
        std::uint16_t base = static_cast<std::uint16_t>(page_size - 2 * h.num_slots);
        for (int i = 0; i < h.num_slots; ++i)
            write_slot(page, base, i, offs[i]);
        h.dir_start = base;
        write_btree_hdr(page, h);
        ph->checksum = 0;
        ph->checksum = ods::crc32c(page.data(), page.size());
    }

    void parse_leaf_page_v1(const std::vector<std::uint8_t>& page,
                            std::vector<LeafRecordV1>& out_records, CompositeKey& out_high_key,
                            std::string& out_per_page_prefix)
    {
        out_records.clear();
        out_high_key.parts.clear();
        out_per_page_prefix.clear();
        auto h = read_btree_hdr(page);
        if (h.prefix_off) {
            std::uint16_t off = h.prefix_off;
            std::uint16_t pl = 0;
            std::memcpy(&pl, page.data() + off, 2);
            off += 2;
            out_per_page_prefix.assign(reinterpret_cast<const char*>(page.data() + off), pl);
        }
        if (h.high_key_off) {
            std::uint16_t off = h.high_key_off;
            decode_key_from(page, off, out_high_key);
        }
        std::uint16_t base = static_cast<std::uint16_t>(page.size() - 2 * h.num_slots);
        for (int i = 0; i < h.num_slots; ++i) {
            std::uint16_t off = read_slot(page, base, i);
            LeafRecordV1 r{};
            decode_key_from(page, off, r.key);
            std::memcpy(&r.row_id, page.data() + off, 8);
            off += 8;
            std::uint16_t pl = 0;
            std::memcpy(&pl, page.data() + off, 2);
            off += 2;
            r.payload.assign(reinterpret_cast<const char*>(page.data() + off), pl);
            out_records.push_back(std::move(r));
        }
    }

    void build_branch_page_v1(std::vector<std::uint8_t>& page, std::uint32_t page_size,
                              const std::vector<BranchEntryV1>& entries,
                              std::uint32_t leftmost_child, const CompositeKey* high_key,
                              const std::string* per_page_prefix)
    {
        page.assign(page_size, 0);
        auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
        set_hdr(*ph, ph->page_no, page_size, ods::PageType::IndexBranch);
        BTreeHdrV1 h{};
        h.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(BTreeHdrV1));
        h.dir_start = static_cast<std::uint16_t>(page_size);
        h.leftmost_child = leftmost_child;
        if (per_page_prefix && !per_page_prefix->empty()) {
            h.prefix_off = h.free_start;
            std::uint16_t pl = static_cast<std::uint16_t>(per_page_prefix->size());
            write16(page, h.free_start, pl);
            h.free_start += 2;
            if (pl) {
                std::memcpy(page.data() + h.free_start, per_page_prefix->data(), pl);
                h.free_start += pl;
            }
            h.flags |= static_cast<std::uint16_t>(BTreePageFlags::BranchPerPagePrefix);
        }
        if (high_key) {
            h.high_key_off = h.free_start;
            encode_key_to(page, h.free_start, *high_key);
        }
        std::vector<std::uint16_t> offs;
        offs.reserve(entries.size());
        for (const auto& e : entries) {
            offs.push_back(h.free_start);
            encode_key_to(page, h.free_start, e.sep_key);
            write32(page, h.free_start, e.child_page);
            h.free_start += 4;
            if (h.free_start + 2 > h.dir_start)
                throw std::runtime_error("branch overflow");
            h.num_slots++;
        }
        std::uint16_t base = static_cast<std::uint16_t>(page_size - 2 * h.num_slots);
        for (int i = 0; i < h.num_slots; ++i)
            write_slot(page, base, i, offs[i]);
        h.dir_start = base;
        write_btree_hdr(page, h);
        ph->checksum = 0;
        ph->checksum = ods::crc32c(page.data(), page.size());
    }

    void parse_branch_page_v1(const std::vector<std::uint8_t>& page,
                              std::vector<BranchEntryV1>& out_entries, CompositeKey& out_high_key,
                              std::string& out_per_page_prefix, std::uint32_t& out_leftmost_child)
    {
        out_entries.clear();
        out_high_key.parts.clear();
        out_per_page_prefix.clear();
        auto h = read_btree_hdr(page);
        out_leftmost_child = h.leftmost_child;
        if (h.prefix_off) {
            std::uint16_t off = h.prefix_off;
            std::uint16_t pl = 0;
            std::memcpy(&pl, page.data() + off, 2);
            off += 2;
            out_per_page_prefix.assign(reinterpret_cast<const char*>(page.data() + off), pl);
        }
        if (h.high_key_off) {
            std::uint16_t off = h.high_key_off;
            decode_key_from(page, off, out_high_key);
        }
        std::uint16_t base = static_cast<std::uint16_t>(page.size() - 2 * h.num_slots);
        for (int i = 0; i < h.num_slots; ++i) {
            std::uint16_t off = read_slot(page, base, i);
            BranchEntryV1 e{};
            decode_key_from(page, off, e.sep_key);
            std::memcpy(&e.child_page, page.data() + off, 4);
            out_entries.push_back(std::move(e));
        }
    }

} // namespace scratchbird::engine
