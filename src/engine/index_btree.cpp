#include "scratchbird/engine/index_btree.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace scratchbird::engine
{

    // ---------------- BTreeBuilder (unchanged except for includes) ----------------
    BTreeBuilder::BTreeBuilder(FileMap fmap, std::uint32_t page_size)
        : fmap_(std::move(fmap)), page_size_(page_size)
    {
        next_alloc_page_ = 100; // reserve low pages; in real impl, allocate via Allocator
    }

    static std::vector<BTreeKeyRef> slice_keys(const std::vector<BTreeKeyRef>& keys, size_t from,
                                               size_t to)
    {
        if (from >= keys.size())
            return {};
        to = std::min(to, keys.size());
        return std::vector<BTreeKeyRef>(keys.begin() + static_cast<long>(from),
                                        keys.begin() + static_cast<long>(to));
    }

    void BTreeBuilder::write_leaf_page(std::uint32_t page_no, const std::vector<BTreeKeyRef>& slice)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = page_no;
        hdr->type = static_cast<std::uint16_t>(ods::PageType::IndexLeaf);
        hdr->page_size = page_size_;
        // naive payload: count + entries (key_len, key_bytes, row_id)
        std::size_t p = 64;
        std::uint32_t cnt = static_cast<std::uint32_t>(slice.size());
        std::memcpy(&page[p], &cnt, sizeof cnt);
        p += sizeof cnt;
        for (const auto& k : slice) {
            std::uint32_t kl = static_cast<std::uint32_t>(k.key.size());
            if (p + sizeof kl + kl + sizeof(std::uint64_t) > page.size())
                break; // guard
            std::memcpy(&page[p], &kl, sizeof kl);
            p += sizeof kl;
            std::memcpy(&page[p], k.key.data(), kl);
            p += kl;
            std::memcpy(&page[p], &k.row_id, sizeof k.row_id);
            p += sizeof k.row_id;
        }
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(page_no, page.data());
    }

    void BTreeBuilder::write_root(const std::vector<std::uint32_t>& child_pages,
                                  std::uint32_t root_page)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = root_page;
        hdr->type = static_cast<std::uint16_t>(ods::PageType::IndexRoot);
        hdr->page_size = page_size_;
        // payload: count + array of child page numbers
        std::size_t p = 64;
        std::uint32_t cnt = static_cast<std::uint32_t>(child_pages.size());
        std::memcpy(&page[p], &cnt, sizeof cnt);
        p += sizeof cnt;
        for (auto child : child_pages) {
            if (p + sizeof child > page.size())
                break;
            std::memcpy(&page[p], &child, sizeof child);
            p += sizeof child;
        }
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(hdr->page_no, page.data());
    }

    BTreeBuildResult BTreeBuilder::build(const std::vector<BTreeKeyRef>& keys)
    {
        if (keys.empty())
            return {0, 0, 0};
        std::size_t payload_start = 64 + 4; // count
        std::size_t per_leaf = 0;
        std::size_t i = 0;
        std::size_t p = payload_start;
        while (i < keys.size()) {
            std::size_t need = 4 + keys[i].key.size() + 8; // len + key + rowid
            if (p + need > page_size_)
                break;
            p += need;
            per_leaf++;
            i++;
        }
        if (per_leaf == 0)
            throw std::runtime_error("key too large for page");
        std::vector<std::uint32_t> leaves;
        std::size_t from = 0;
        while (from < keys.size()) {
            std::size_t to = std::min(from + per_leaf, keys.size());
            std::uint32_t leaf_page = next_alloc_page_++;
            write_leaf_page(leaf_page, slice_keys(keys, from, to));
            leaves.push_back(leaf_page);
            from = to;
        }
        std::uint32_t root_page = next_alloc_page_++;
        write_root(leaves, root_page);
        return {root_page, static_cast<std::uint32_t>(leaves.size()),
                static_cast<std::uint32_t>(keys.size())};
    }

    // ---------------- Stage 1 BTreeIndex ----------------

    BTreeIndex::BTreeIndex(FileMap fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique)
    {
        next_alloc_page_ = 1; // start from 1 to keep segment count low in tests
    }

    std::vector<std::uint8_t> BTreeIndex::new_page_buffer(ods::PageType t,
                                                          std::uint32_t page_no) const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = page_no;
        hdr->type = static_cast<std::uint16_t>(t);
        hdr->page_size = page_size_;
        return page;
    }

    void BTreeIndex::write_page(std::uint32_t page_no, std::vector<std::uint8_t>& page)
    {
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(page_no, page.data());
    }

    void BTreeIndex::read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const
    {
        fmap_.read_page(page_no, page.data());
    }

    void BTreeIndex::maybe_prefetch(std::uint32_t page_no) const
    {
        if (tunables_.prefetch_horizon_pages == 0)
            return;
        auto [segIdx, segOff] = fmap_.map(page_no);
        if (segIdx >= fmap_.segments().size())
            return;
        const auto& seg = fmap_.segments()[segIdx];
        std::uint64_t file_offset = segOff;
        FileManager::prefetch_willneed(seg.handle, file_offset,
                                       static_cast<std::size_t>(tunables_.prefetch_horizon_pages) *
                                           page_size_);
    }

    BTreeIndex::LeafHdr BTreeIndex::read_leaf_hdr(const std::vector<std::uint8_t>& page)
    {
        LeafHdr h{};
        std::memcpy(&h, page.data() + sizeof(ods::PageHeader), sizeof(LeafHdr));
        return h;
    }

    void BTreeIndex::write_leaf_hdr(std::vector<std::uint8_t>& page, const LeafHdr& h)
    {
        std::memcpy(page.data() + sizeof(ods::PageHeader), &h, sizeof(LeafHdr));
    }

    std::uint16_t BTreeIndex::read_slot(const std::vector<std::uint8_t>& page, const LeafHdr& h,
                                        int index)
    {
        // Slots occupy [base_dir, base_dir + 2*num_slots)
        std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * h.num_slots);
        std::uint16_t val = 0;
        std::memcpy(&val, page.data() + base_dir + 2 * index, 2);
        return val;
    }

    void BTreeIndex::write_slot(std::vector<std::uint8_t>& page, const LeafHdr& h, int index,
                                std::uint16_t value)
    {
        std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * h.num_slots);
        std::memcpy(page.data() + base_dir + 2 * index, &value, 2);
    }

    int BTreeIndex::keycmp(const std::string& a, const std::string& b)
    {
        // Fast path: compare shared prefix bytes via memcmp if both data pointers are available
        const std::size_t na = a.size();
        const std::size_t nb = b.size();
        const std::size_t n = na < nb ? na : nb;
        if (n) {
            int r = std::memcmp(a.data(), b.data(), n);
            if (r < 0)
                return -1;
            if (r > 0)
                return 1;
        }
        // If prefixes equal, shorter key sorts first
        if (na < nb)
            return -1;
        if (na > nb)
            return 1;
        return 0;
    }

    void BTreeIndex::create_empty()
    {
        children_.clear();
        std::uint32_t leaf = next_alloc_page_++;
        auto page = new_page_buffer(ods::PageType::IndexLeaf, leaf);
        LeafHdr lh{};
        lh.num_slots = 0;
        lh.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(LeafHdr));
        lh.dir_start = static_cast<std::uint16_t>(page_size_);
        write_leaf_hdr(page, lh);
        write_page(leaf, page);
        root_page_ = next_alloc_page_++;
        children_.push_back(RootChild{"", leaf});
        write_root();
    }

    void BTreeIndex::write_root()
    {
        auto page = new_page_buffer(ods::PageType::IndexRoot, root_page_);
        std::size_t p = sizeof(ods::PageHeader);
        std::uint16_t cnt = static_cast<std::uint16_t>(children_.size());
        std::memcpy(&page[p], &cnt, sizeof cnt);
        p += sizeof cnt;
        for (const auto& c : children_) {
            std::uint16_t kl = static_cast<std::uint16_t>(c.min_key.size());
            std::memcpy(&page[p], &kl, sizeof kl);
            p += sizeof kl;
            if (kl) {
                std::memcpy(&page[p], c.min_key.data(), kl);
                p += kl;
            }
            std::memcpy(&page[p], &c.page_no, sizeof c.page_no);
            p += sizeof c.page_no;
        }
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(root_page_, page.data());
    }

    void BTreeIndex::read_root()
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_.read_page(root_page_, page.data());
        std::size_t p = sizeof(ods::PageHeader);
        std::uint16_t cnt = 0;
        std::memcpy(&cnt, &page[p], sizeof cnt);
        p += sizeof cnt;
        children_.clear();
        children_.reserve(cnt);
        for (int i = 0; i < cnt; ++i) {
            std::uint16_t kl = 0;
            std::memcpy(&kl, &page[p], sizeof kl);
            p += sizeof kl;
            std::string key(kl, '\0');
            if (kl) {
                std::memcpy(key.data(), &page[p], kl);
                p += kl;
            }
            std::uint32_t pg = 0;
            std::memcpy(&pg, &page[p], sizeof pg);
            p += sizeof pg;
            children_.push_back(RootChild{std::move(key), pg});
        }
    }

    int BTreeIndex::find_child_for_key(const std::string& key) const
    {
        int idx = static_cast<int>(children_.size()) - 1;
        for (int i = 0; i < static_cast<int>(children_.size()); ++i) {
            if (keycmp(children_[i].min_key, key) <= 0)
                idx = i;
            else
                break;
        }
        return std::max(0, idx);
    }

    void BTreeIndex::insert_root_child(int after_index, const std::string& split_key,
                                       std::uint32_t new_page)
    {
        RootChild c{split_key, new_page};
        children_.insert(children_.begin() + after_index + 1, c);
        write_root();
    }

    bool BTreeIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        int idx = find_child_for_key(key);
        std::string split_key;
        std::uint32_t newp = 0;
        static const std::string empty_payload;
        if (!leaf_insert_common(children_[idx].page_no, key, row_id, empty_payload, err, &split_key,
                                &newp))
            return false;
        if (newp) {
            insert_root_child(idx, split_key, newp);
            children_[idx + 1].min_key = split_key;
            write_root();
        }
        return true;
    }

    bool BTreeIndex::leaf_insert(std::uint32_t page_no, const std::string& key,
                                 std::uint64_t row_id, std::string& err, std::string* split_key,
                                 std::uint32_t* new_page_out)
    {
        static const std::string empty_payload;
        return leaf_insert_common(page_no, key, row_id, empty_payload, err, split_key,
                                  new_page_out);
    }

    bool BTreeIndex::leaf_insert_common(std::uint32_t page_no, const std::string& key,
                                        std::uint64_t row_id, const std::string& payload,
                                        std::string& err, std::string* split_key,
                                        std::uint32_t* new_page_out)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        read_page(page_no, page);
        auto lh = read_leaf_hdr(page);
        int lo = 0, hi = lh.num_slots;
        int pos = 0;
        bool found = false;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            std::uint16_t base_dir_old = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
            std::uint16_t off = 0;
            std::memcpy(&off, page.data() + base_dir_old + 2 * mid, 2);
            std::string mk;
            std::uint64_t rid{};
            std::string pl;
            read_record(page, off, mk, rid, pl);
            int cmp = keycmp(key, mk);
            if (cmp == 0) {
                pos = mid;
                found = true;
                break;
            }
            if (cmp < 0)
                hi = mid;
            else
                lo = mid + 1;
        }
        if (!found)
            pos = lo;
        if (unique_ && found) {
            err = "duplicate key";
            return false;
        }
        std::uint16_t need = static_cast<std::uint16_t>(record_size(key, payload) + 2);
        if (lh.free_start + need > lh.dir_start) {
            // split path: gather records, insert, split evenly
            std::vector<std::tuple<std::string, std::uint64_t, std::string>> recs;
            recs.reserve(lh.num_slots + 1);
            for (int i = 0; i < lh.num_slots; ++i) {
                std::uint16_t base_dir_old =
                    static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
                std::uint16_t off = 0;
                std::memcpy(&off, page.data() + base_dir_old + 2 * i, 2);
                std::string mk;
                std::uint64_t rid{};
                std::string pl;
                read_record(page, off, mk, rid, pl);
                recs.emplace_back(std::move(mk), rid, std::move(pl));
            }
            recs.emplace_back(key, row_id, payload);
            std::sort(recs.begin(), recs.end(),
                      [](auto& a, auto& b) { return std::get<0>(a) < std::get<0>(b); });
            size_t mid = recs.size() / 2;
            if (tunables_.split_policy == BTreeTunables::SplitPolicy::LeftBiased) {
                mid = static_cast<size_t>(recs.size() * tunables_.fillfactor);
            } else if (tunables_.split_policy == BTreeTunables::SplitPolicy::RightBiased) {
                mid = static_cast<size_t>(recs.size() * (1.0 - tunables_.fillfactor));
            }
            std::vector<std::tuple<std::string, std::uint64_t, std::string>> left(
                recs.begin(), recs.begin() + mid);
            std::vector<std::tuple<std::string, std::uint64_t, std::string>> right(
                recs.begin() + mid, recs.end());
            // rewrite left
            lh.num_slots = 0;
            lh.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(LeafHdr));
            lh.dir_start = static_cast<std::uint16_t>(page_size_);
            write_leaf_hdr(page, lh);
            std::vector<std::uint16_t> left_offs;
            left_offs.reserve(left.size());
            for (auto& e : left) {
                std::uint16_t start_off = lh.free_start;
                auto& k = std::get<0>(e);
                auto& r = std::get<1>(e);
                auto& pl = std::get<2>(e);
                write_record(page, lh.free_start, k, r, pl);
                left_offs.push_back(start_off);
                lh.num_slots++;
            }
            std::uint16_t base_new = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
            for (int i = 0; i < lh.num_slots; ++i)
                std::memcpy(page.data() + base_new + 2 * i, &left_offs[i], 2);
            lh.dir_start = base_new;
            write_leaf_hdr(page, lh);
            write_page(page_no, page);
            // right page
            std::uint32_t newp = next_alloc_page_++;
            auto rpage = new_page_buffer(ods::PageType::IndexLeaf, newp);
            LeafHdr rh{};
            rh.num_slots = 0;
            rh.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(LeafHdr));
            rh.dir_start = static_cast<std::uint16_t>(page_size_);
            write_leaf_hdr(rpage, rh);
            std::vector<std::uint16_t> right_offs;
            right_offs.reserve(right.size());
            for (auto& e : right) {
                std::uint16_t start_off = rh.free_start;
                auto& k = std::get<0>(e);
                auto& r = std::get<1>(e);
                auto& pl = std::get<2>(e);
                write_record(rpage, rh.free_start, k, r, pl);
                right_offs.push_back(start_off);
                rh.num_slots++;
            }
            std::uint16_t rbase = static_cast<std::uint16_t>(rpage.size() - 2 * rh.num_slots);
            for (int i = 0; i < rh.num_slots; ++i)
                std::memcpy(rpage.data() + rbase + 2 * i, &right_offs[i], 2);
            rh.dir_start = rbase;
            write_leaf_hdr(rpage, rh);
            write_page(newp, rpage);
            if (split_key)
                *split_key = std::get<0>(right.front());
            if (new_page_out)
                *new_page_out = newp;
            return true;
        }
        // no split; write record and rebuild dir
        std::uint16_t start_off = lh.free_start;
        write_record(page, lh.free_start, key, row_id, payload);
        std::vector<std::uint16_t> offs(lh.num_slots);
        std::uint16_t base_dir_old = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
        for (int i = 0; i < lh.num_slots; ++i)
            std::memcpy(&offs[i], page.data() + base_dir_old + 2 * i, 2);
        offs.insert(offs.begin() + pos, start_off);
        lh.num_slots++;
        std::uint16_t base_new = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
        for (int i = 0; i < lh.num_slots; ++i)
            std::memcpy(page.data() + base_new + 2 * i, &offs[i], 2);
        lh.dir_start = base_new;
        write_leaf_hdr(page, lh);
        write_page(page_no, page);
        if (split_key)
            *split_key = std::string();
        if (new_page_out)
            *new_page_out = 0;
        return true;
    }

    bool BTreeIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                         const std::string& payload, std::string& err)
    {
        int idx = find_child_for_key(key);
        std::string split_key;
        std::uint32_t newp = 0;
        if (!leaf_insert_common(children_[idx].page_no, key, row_id, payload, err, &split_key,
                                &newp))
            return false;
        if (newp) {
            insert_root_child(idx, split_key, newp);
            children_[idx + 1].min_key = split_key;
            write_root();
        }
        return true;
    }

    void BTreeIndex::leaf_scan_equal(std::uint32_t page_no, const std::string& key,
                                     std::vector<std::uint64_t>& out) const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        maybe_prefetch(page_no);
        read_page(page_no, page);
        auto lh = read_leaf_hdr(page);
        for (int i = 0; i < lh.num_slots; ++i) {
            std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
            std::uint16_t off = 0;
            std::memcpy(&off, page.data() + base_dir + 2 * i, 2);
            std::uint16_t kl = 0;
            std::memcpy(&kl, &page[off], 2);
            std::string mk(kl, '\0');
            if (kl)
                std::memcpy(mk.data(), &page[off + 2], kl);
            if (keycmp(key, mk) == 0) {
                std::uint64_t rid = 0;
                std::memcpy(&rid, &page[off + 2 + kl], 8);
                out.push_back(rid);
            }
        }
    }

    void BTreeIndex::search_equal(const std::string& key, std::vector<std::uint64_t>& out) const
    {
        int idx = find_child_for_key(key);
        leaf_scan_equal(children_[idx].page_no, key, out);
    }

    void BTreeIndex::leaf_scan_range(std::uint32_t page_no, const std::string& lo, bool lo_incl,
                                     const std::string& hi, bool hi_incl,
                                     std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        maybe_prefetch(page_no);
        read_page(page_no, page);
        auto lh = read_leaf_hdr(page);
        for (int i = 0; i < lh.num_slots; ++i) {
            std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
            std::uint16_t off = 0;
            std::memcpy(&off, page.data() + base_dir + 2 * i, 2);
            std::uint16_t kl = 0;
            std::memcpy(&kl, &page[off], 2);
            std::string mk(kl, '\0');
            if (kl)
                std::memcpy(mk.data(), &page[off + 2], kl);
            bool ge_lo = lo.empty() || (lo_incl ? keycmp(mk, lo) >= 0 : keycmp(mk, lo) > 0);
            bool le_hi = hi.empty() || (hi_incl ? keycmp(mk, hi) <= 0 : keycmp(mk, hi) < 0);
            if (ge_lo && le_hi) {
                std::uint64_t rid = 0;
                std::memcpy(&rid, &page[off + 2 + kl], 8);
                out.emplace_back(std::move(mk), rid);
            }
        }
    }

    void BTreeIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                  bool hi_incl,
                                  std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        for (const auto& c : children_) {
            leaf_scan_range(c.page_no, lo, lo_incl, hi, hi_incl, out);
        }
    }

    std::string BTreeIndex::read_first_key(std::uint32_t page_no) const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        read_page(page_no, page);
        auto lh = read_leaf_hdr(page);
        if (lh.num_slots == 0)
            return std::string();
        std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
        std::uint16_t off = 0;
        std::memcpy(&off, page.data() + base_dir + 0, 2);
        std::uint16_t kl = 0;
        std::memcpy(&kl, &page[off], 2);
        std::string mk(kl, '\0');
        if (kl)
            std::memcpy(mk.data(), &page[off + 2], kl);
        return mk;
    }

    bool BTreeIndex::leaf_erase_rewrite(std::uint32_t page_no, const std::string& key,
                                        std::size_t& removed, std::string& new_first_key)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        read_page(page_no, page);
        auto lh = read_leaf_hdr(page);
        if (lh.num_slots == 0)
            return false;
        // Gather entries except matching key
        std::vector<std::pair<std::string, std::uint64_t>> entries;
        entries.reserve(lh.num_slots);
        for (int i = 0; i < lh.num_slots; ++i) {
            std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
            std::uint16_t off = 0;
            std::memcpy(&off, page.data() + base_dir + 2 * i, 2);
            std::uint16_t kl = 0;
            std::memcpy(&kl, &page[off], 2);
            std::string mk(kl, '\0');
            if (kl)
                std::memcpy(mk.data(), &page[off + 2], kl);
            std::uint64_t rid = 0;
            std::memcpy(&rid, &page[off + 2 + kl], 8);
            if (keycmp(mk, key) == 0) {
                removed++;
                continue;
            }
            entries.emplace_back(std::move(mk), rid);
        }
        // Rewrite page
        LeafHdr nh{};
        nh.num_slots = 0;
        nh.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(LeafHdr));
        nh.dir_start = static_cast<std::uint16_t>(page_size_);
        write_leaf_hdr(page, nh);
        std::vector<std::uint16_t> offs;
        offs.reserve(entries.size());
        for (auto& e : entries) {
            std::uint16_t start_off = nh.free_start;
            std::uint16_t kl2 = static_cast<std::uint16_t>(e.first.size());
            std::memcpy(&page[nh.free_start], &kl2, 2);
            nh.free_start += 2;
            if (kl2) {
                std::memcpy(&page[nh.free_start], e.first.data(), kl2);
                nh.free_start += kl2;
            }
            std::memcpy(&page[nh.free_start], &e.second, 8);
            nh.free_start += 8;
            offs.push_back(start_off);
            nh.num_slots++;
        }
        std::uint16_t base_new = static_cast<std::uint16_t>(page.size() - 2 * nh.num_slots);
        for (int i = 0; i < nh.num_slots; ++i) {
            std::memcpy(page.data() + base_new + 2 * i, &offs[i], 2);
        }
        nh.dir_start = base_new;
        write_leaf_hdr(page, nh);
        write_page(page_no, page);
        new_first_key = nh.num_slots ? entries.front().first : std::string();
        return true;
    }

    bool BTreeIndex::try_merge_with_right(std::size_t child_index)
    {
        if (child_index + 1 >= children_.size())
            return false;
        std::uint32_t left = children_[child_index].page_no;
        std::uint32_t right = children_[child_index + 1].page_no;
        std::vector<std::uint8_t> lpage(page_size_, 0), rpage(page_size_, 0);
        read_page(left, lpage);
        read_page(right, rpage);
        auto lh = read_leaf_hdr(lpage);
        auto rh = read_leaf_hdr(rpage);
        // Compute if right entries fit into left
        // Approximate size required
        std::size_t bytes_left = lh.free_start - (page_size_ - 2 * lh.num_slots);
        std::size_t bytes_right = rh.free_start - (page_size_ - 2 * rh.num_slots);
        if (sizeof(ods::PageHeader) + sizeof(LeafHdr) + bytes_left + bytes_right +
                2 * (lh.num_slots + rh.num_slots) >
            page_size_)
            return false;
        // Gather both, rewrite into left, clear right (optional)
        std::vector<std::pair<std::string, std::uint64_t>> entries;
        entries.reserve(lh.num_slots + rh.num_slots);
        auto gather = [&](const std::vector<std::uint8_t>& page, LeafHdr hdr) {
            for (int i = 0; i < hdr.num_slots; ++i) {
                std::uint16_t base_dir =
                    static_cast<std::uint16_t>(page.size() - 2 * hdr.num_slots);
                std::uint16_t off = 0;
                std::memcpy(&off, page.data() + base_dir + 2 * i, 2);
                std::uint16_t kl = 0;
                std::memcpy(&kl, &page[off], 2);
                std::string mk(kl, '\0');
                if (kl)
                    std::memcpy(mk.data(), &page[off + 2], kl);
                std::uint64_t rid = 0;
                std::memcpy(&rid, &page[off + 2 + kl], 8);
                entries.emplace_back(std::move(mk), rid);
            }
        };
        gather(lpage, lh);
        gather(rpage, rh);
        std::sort(entries.begin(), entries.end(),
                  [](auto& a, auto& b) { return a.first < b.first; });
        LeafHdr nh{};
        nh.num_slots = 0;
        nh.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(LeafHdr));
        nh.dir_start = static_cast<std::uint16_t>(page_size_);
        write_leaf_hdr(lpage, nh);
        std::vector<std::uint16_t> offs;
        offs.reserve(entries.size());
        for (auto& e : entries) {
            std::uint16_t start_off = nh.free_start;
            std::uint16_t kl2 = static_cast<std::uint16_t>(e.first.size());
            std::memcpy(&lpage[nh.free_start], &kl2, 2);
            nh.free_start += 2;
            if (kl2) {
                std::memcpy(&lpage[nh.free_start], e.first.data(), kl2);
                nh.free_start += kl2;
            }
            std::memcpy(&lpage[nh.free_start], &e.second, 8);
            nh.free_start += 8;
            offs.push_back(start_off);
            nh.num_slots++;
        }
        std::uint16_t base_new = static_cast<std::uint16_t>(lpage.size() - 2 * nh.num_slots);
        for (int i = 0; i < nh.num_slots; ++i)
            std::memcpy(lpage.data() + base_new + 2 * i, &offs[i], 2);
        nh.dir_start = base_new;
        write_leaf_hdr(lpage, nh);
        write_page(left, lpage);
        // Remove right child from root children
        children_.erase(children_.begin() + child_index + 1);
        write_root();
        return true;
    }

    std::size_t BTreeIndex::erase_equal(const std::string& key, std::string& err)
    {
        (void)err;
        std::size_t removed = 0;
        // delete from all children whose range may include key; in this simple root, scan all
        for (std::size_t i = 0; i < children_.size(); ++i) {
            std::string new_first;
            bool touched = leaf_erase_rewrite(children_[i].page_no, key, removed, new_first);
            if (touched) {
                // Update min_key if first key changed
                if (!new_first.empty() && children_[i].min_key != new_first) {
                    children_[i].min_key = new_first;
                    write_root();
                }
                // Try merge with right sibling if current became sparse
                try_merge_with_right(i);
            }
        }
        return removed;
    }

    // -------- Stage 3: payload and maintenance --------

    std::uint16_t BTreeIndex::record_size(const std::string& key, const std::string& payload)
    {
        return static_cast<std::uint16_t>(2 + key.size() + 8 + 2 + payload.size());
    }

    void BTreeIndex::write_record(std::vector<std::uint8_t>& page, std::uint16_t& at,
                                  const std::string& key, std::uint64_t row_id,
                                  const std::string& payload)
    {
        std::uint16_t kl = static_cast<std::uint16_t>(key.size());
        std::uint16_t pl = static_cast<std::uint16_t>(payload.size());
        std::memcpy(&page[at], &kl, 2);
        at += 2;
        if (kl) {
            std::memcpy(&page[at], key.data(), kl);
            at += kl;
        }
        std::memcpy(&page[at], &row_id, 8);
        at += 8;
        std::memcpy(&page[at], &pl, 2);
        at += 2;
        if (pl) {
            std::memcpy(&page[at], payload.data(), pl);
            at += pl;
        }
    }

    void BTreeIndex::read_record(const std::vector<std::uint8_t>& page, std::uint16_t off,
                                 std::string& key, std::uint64_t& row_id, std::string& payload)
    {
        std::uint16_t kl = 0;
        std::memcpy(&kl, &page[off], 2);
        off += 2;
        key.assign(kl, '\0');
        if (kl)
            std::memcpy(key.data(), &page[off], kl);
        off += kl;
        std::memcpy(&row_id, &page[off], 8);
        off += 8;
        std::uint16_t pl = 0;
        std::memcpy(&pl, &page[off], 2);
        off += 2;
        payload.assign(pl, '\0');
        if (pl)
            std::memcpy(payload.data(), &page[off], pl);
    }

    void BTreeIndex::leaf_scan_equal_payload(
        std::uint32_t page_no, const std::string& key,
        std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        maybe_prefetch(page_no);
        read_page(page_no, page);
        auto lh = read_leaf_hdr(page);
        for (int i = 0; i < lh.num_slots; ++i) {
            std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
            std::uint16_t off = 0;
            std::memcpy(&off, page.data() + base_dir + 2 * i, 2);
            std::string mk;
            std::uint64_t rid{};
            std::string pl;
            read_record(page, off, mk, rid, pl);
            if (keycmp(key, mk) == 0)
                out.emplace_back(rid, std::move(pl));
        }
    }

    void BTreeIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        int idx = find_child_for_key(key);
        leaf_scan_equal_payload(children_[idx].page_no, key, out);
    }

    BTreeStats BTreeIndex::compute_stats() const
    {
        BTreeStats s{};
        s.height = 2; // root + leaves (no branches yet)
        s.leaf_pages = static_cast<std::uint32_t>(children_.size());
        s.branch_pages = 0;
        s.key_count = 0;
        s.min_key = children_.empty() ? std::string() : read_first_key(children_.front().page_no);
        s.max_key.clear();
        for (const auto& c : children_) {
            std::vector<std::uint8_t> page(page_size_, 0);
            read_page(c.page_no, page);
            auto lh = read_leaf_hdr(page);
            s.key_count += lh.num_slots;
            if (lh.num_slots) {
                std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
                std::uint16_t off = 0;
                std::memcpy(&off, page.data() + base_dir + 2 * (lh.num_slots - 1), 2);
                std::string mk;
                std::uint64_t rid{};
                std::string pl;
                read_record(page, off, mk, rid, pl);
                s.max_key = mk;
            }
        }
        return s;
    }

    void BTreeIndex::rebuild_offline()
    {
        // Read all keys/payloads, sort, and rewrite into fresh leaf pages and root.
        std::vector<std::tuple<std::string, std::uint64_t, std::string>> recs;
        for (const auto& c : children_) {
            std::vector<std::uint8_t> page(page_size_, 0);
            read_page(c.page_no, page);
            auto lh = read_leaf_hdr(page);
            for (int i = 0; i < lh.num_slots; ++i) {
                std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
                std::uint16_t off = 0;
                std::memcpy(&off, page.data() + base_dir + 2 * i, 2);
                std::string mk;
                std::uint64_t rid{};
                std::string pl;
                read_record(page, off, mk, rid, pl);
                recs.emplace_back(std::move(mk), rid, std::move(pl));
            }
        }
        std::sort(recs.begin(), recs.end(),
                  [](auto& a, auto& b) { return std::get<0>(a) < std::get<0>(b); });
        // Reset structure
        children_.clear();
        std::uint32_t first_leaf = next_alloc_page_++;
        auto page = new_page_buffer(ods::PageType::IndexLeaf, first_leaf);
        LeafHdr lh{};
        lh.num_slots = 0;
        lh.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(LeafHdr));
        lh.dir_start = static_cast<std::uint16_t>(page_size_);
        write_leaf_hdr(page, lh);
        write_page(first_leaf, page);
        root_page_ = next_alloc_page_++;
        children_.push_back(RootChild{"", first_leaf});
        write_root();
        // Reinsert into fresh structure with payloads
        std::string err;
        for (auto& e : recs) {
            insert_with_payload(std::get<0>(e), std::get<1>(e), std::get<2>(e), err);
        }
    }

    bool BTreeIndex::validate(std::string& error) const
    {
        error.clear();
        std::string global_prev;
        bool has_prev = false;
        for (std::size_t ci = 0; ci < children_.size(); ++ci) {
            std::vector<std::uint8_t> page(page_size_, 0);
            read_page(children_[ci].page_no, page);
            auto* hdr = reinterpret_cast<const ods::PageHeader*>(page.data());
            if (hdr->type != static_cast<std::uint16_t>(ods::PageType::IndexLeaf)) {
                error = "root child not a leaf";
                return false;
            }
            // checksum
            std::vector<std::uint8_t> tmp = page;
            reinterpret_cast<ods::PageHeader*>(tmp.data())->checksum = 0;
            if (hdr->checksum != ods::crc32c(tmp.data(), tmp.size())) {
                error = "checksum mismatch";
                return false;
            }
            auto lh = read_leaf_hdr(page);
            std::string prev;
            bool has_local_prev = false;
            for (int i = 0; i < lh.num_slots; ++i) {
                std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
                std::uint16_t off = 0;
                std::memcpy(&off, page.data() + base_dir + 2 * i, 2);
                std::uint16_t kl = 0;
                std::memcpy(&kl, &page[off], 2);
                std::string mk(kl, '\0');
                if (kl)
                    std::memcpy(mk.data(), &page[off + 2], kl);
                if (has_local_prev && keycmp(prev, mk) > 0) {
                    error = "leaf keys out of order";
                    return false;
                }
                if (has_prev && keycmp(global_prev, mk) > 0) {
                    error = "global leaf ordering violation";
                    return false;
                }
                prev = mk;
                has_local_prev = true;
                global_prev = mk;
                has_prev = true;
            }
        }
        // root min_key vs first key
        for (std::size_t ci = 0; ci < children_.size(); ++ci) {
            std::vector<std::uint8_t> page(page_size_, 0);
            read_page(children_[ci].page_no, page);
            auto lh = read_leaf_hdr(page);
            if (lh.num_slots == 0)
                continue;
            std::uint16_t base_dir = static_cast<std::uint16_t>(page.size() - 2 * lh.num_slots);
            std::uint16_t off = 0;
            std::memcpy(&off, page.data() + base_dir + 0, 2);
            std::uint16_t kl = 0;
            std::memcpy(&kl, &page[off], 2);
            std::string first(kl, '\0');
            if (kl)
                std::memcpy(first.data(), &page[off + 2], kl);
            if (keycmp(children_[ci].min_key, first) > 0) {
                error = "root min_key greater than first key";
                return false;
            }
        }
        return true;
    }
} // namespace scratchbird::engine
