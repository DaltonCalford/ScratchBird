#include "scratchbird/engine/btree_v1.h"

#include "scratchbird/engine/config.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace scratchbird::engine
{

    BTreeV1::BTreeV1(FileMap fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size),
          cache_(std::make_shared<BufferCache>(page_size_, /*capacity_pages*/ 1024)),
          pager_(&fmap_, cache_), allocator_(&fmap_, page_size_), unique_(unique)
    {
    }

    void BTreeV1::read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const
    {
        page.resize(page_size_);
        PageKey k{1, page_no};
        auto* f = pager_.get_page(k, LatchMode::Shared);
        page.assign(f->data.begin(), f->data.end());
        pager_.release(f);
    }

    void BTreeV1::write_page(std::uint32_t page_no, std::vector<std::uint8_t>& page)
    {
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = page_no;
        hdr->page_size = page_size_;
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        PageKey k{1, page_no};
        auto* f = pager_.get_page(k, LatchMode::Exclusive);
        std::memcpy(f->data.data(), page.data(), page.size());
        pager_.mark_dirty(f);
        pager_.release(f);
    }

    std::uint32_t BTreeV1::alloc_page()
    {
        return allocator_.allocate_free_page();
    }

    void BTreeV1::create_empty()
    {
        root_is_leaf_ = true;
        root_page_ = alloc_page();
        std::vector<std::uint8_t> page(page_size_, 0);
        auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
        ph->type = static_cast<std::uint16_t>(ods::PageType::IndexLeaf);
        BTreeHdrV1 h{};
        h.free_start = static_cast<std::uint16_t>(sizeof(ods::PageHeader) + sizeof(BTreeHdrV1));
        h.dir_start = page_size_;
        std::memcpy(page.data() + sizeof(ods::PageHeader), &h, sizeof h);
        write_page(root_page_, page);
    }

    int BTreeV1::cmp_key(const CompositeKey& a, const CompositeKey& b) const
    {
        size_t np = std::min(a.parts.size(), b.parts.size());
        for (size_t i = 0; i < np; ++i) {
            const auto& pa = a.parts[i];
            const auto& pb = b.parts[i];
            bool desc = (i < field_orders_.size() ? field_orders_[i].desc : false);
            bool nulls_first = (i < field_orders_.size() ? field_orders_[i].nulls_first : true);
            bool ci = (i < field_orders_.size() ? field_orders_[i].case_insensitive : false);
            if (pa.is_null || pb.is_null) {
                if (pa.is_null && pb.is_null)
                    continue;
                int nn = (pa.is_null ? (nulls_first ? -1 : 1) : (nulls_first ? 1 : -1));
                return desc ? -nn : nn;
            }
            int cmp = 0;
            if (ci) {
                std::string as = pa.bytes, bs = pb.bytes;
                // basic ASCII fold; full ICU not wired here
                for (auto& ch : as)
                    ch = char(std::tolower(static_cast<unsigned char>(ch)));
                for (auto& ch : bs)
                    ch = char(std::tolower(static_cast<unsigned char>(ch)));
                cmp = as.compare(bs);
            } else {
                cmp = pa.bytes.compare(pb.bytes);
            }
            if (cmp != 0)
                return desc ? -cmp : cmp;
        }
        if (a.parts.size() == b.parts.size())
            return 0;
        return a.parts.size() < b.parts.size() ? -1 : 1;
    }

    std::string BTreeV1::key_to_string(const CompositeKey& k)
    {
        std::string out;
        for (size_t i = 0; i < k.parts.size(); ++i) {
            if (i)
                out.push_back('|');
            if (k.parts[i].is_null)
                out += "<NULL>";
            else
                out += k.parts[i].bytes;
        }
        return out;
    }

    BTreeV1::InsertUp BTreeV1::insert_into_leaf(std::uint32_t page_no, const CompositeKey& key,
                                                std::uint64_t row_id, const std::string& payload)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        read_page(page_no, page);
        auto* ph_orig = reinterpret_cast<ods::PageHeader*>(page.data());
        std::uint32_t prev_link = ph_orig->prev;
        std::uint32_t next_link = ph_orig->next;
        std::uint64_t lsn = 0;
        if (wal_) {
            std::vector<std::uint8_t> key_enc;
            detail::encode_key(key, key_enc);
            lsn = wal_->append_insert(key_enc, row_id, payload);
        }
        std::vector<LeafRecordV1> recs;
        CompositeKey hk;
        std::string pref;
        parse_leaf_page_v1(page, recs, hk, pref);
        // insert in sorted order
        auto it = std::lower_bound(
            recs.begin(), recs.end(), key,
            [&](const LeafRecordV1& r, const CompositeKey& k) { return cmp_key(r.key, k) < 0; });
        // Partial index predicate enforcement
        if (predicate_) {
            if (!predicate_(key, row_id, payload)) {
                return InsertUp{}; // not indexed
            }
        }
        if (unique_ && it != recs.end() && cmp_key(it->key, key) == 0)
            return InsertUp{}; // duplicate silently ignored for test; real code sets error
        // Additional duplicate probe path for concurrent insert windows: scan neighbors equal
        if (unique_) {
            if (it != recs.begin()) {
                auto pit = it;
                --pit;
                if (cmp_key(pit->key, key) == 0)
                    return InsertUp{};
            }
            if (it != recs.end()) {
                auto nit = it;
                if (nit != recs.end() && cmp_key(nit->key, key) == 0)
                    return InsertUp{};
            }
        }
        recs.insert(it, LeafRecordV1{key, row_id, payload});
        // naive split threshold: half-full
        if (recs.size() * 32 > page_size_) {
            size_t mid = recs.size() / 2;
            std::vector<LeafRecordV1> left(recs.begin(), recs.begin() + mid);
            std::vector<LeafRecordV1> right(recs.begin() + mid, recs.end());
            std::vector<std::uint8_t> lpage(page_size_, 0), rpage(page_size_, 0);
            CompositeKey right_high{}; // none
            build_leaf_page_v1(lpage, page_size_, left, nullptr, nullptr);
            build_leaf_page_v1(rpage, page_size_, right, nullptr, nullptr);
            // allocate right page and wire siblings
            std::uint32_t right_no = alloc_page();
            auto* lhdr = reinterpret_cast<ods::PageHeader*>(lpage.data());
            auto* rhdr = reinterpret_cast<ods::PageHeader*>(rpage.data());
            lhdr->type = static_cast<std::uint16_t>(ods::PageType::IndexLeaf);
            lhdr->prev = prev_link;
            lhdr->next = right_no;
            rhdr->type = static_cast<std::uint16_t>(ods::PageType::IndexLeaf);
            rhdr->prev = page_no;
            rhdr->next = next_link;
            // set scn and write left into original page_no
            auto* lh = reinterpret_cast<ods::PageHeader*>(lpage.data());
            lh->scn = lsn;
            write_page(page_no, lpage);
            // write right page
            auto* rh = reinterpret_cast<ods::PageHeader*>(rpage.data());
            rh->scn = lsn;
            write_page(right_no, rpage);
            // fix next neighbor's prev link
            if (next_link) {
                std::vector<std::uint8_t> npage(page_size_, 0);
                read_page(next_link, npage);
                auto* nh = reinterpret_cast<ods::PageHeader*>(npage.data());
                nh->prev = right_no;
                write_page(next_link, npage);
            }
            // new separator: first key of right
            InsertUp up{};
            up.has = true;
            up.sep = right.front().key;
            up.right_child = right_no;
            return up;
        } else {
            // rebuild but keep links
            build_leaf_page_v1(page, page_size_, recs, nullptr, nullptr);
            auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
            ph->scn = lsn;
            ph->prev = prev_link;
            ph->next = next_link;
            write_page(page_no, page);
            return InsertUp{};
        }
    }

    BTreeV1::InsertUp BTreeV1::insert_into_branch(std::uint32_t page_no, const CompositeKey& sep,
                                                  std::uint32_t child_page)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        read_page(page_no, page);
        std::vector<BranchEntryV1> ents;
        CompositeKey hk;
        std::string pref;
        std::uint32_t leftmost = 0;
        parse_branch_page_v1(page, ents, hk, pref, leftmost);
        // find insert pos by sep
        auto it = std::lower_bound(ents.begin(), ents.end(), sep,
                                   [&](const BranchEntryV1& e, const CompositeKey& k) {
                                       return cmp_key(e.sep_key, k) < 0;
                                   });
        ents.insert(it, BranchEntryV1{sep, child_page});
        if (ents.size() * 28 > page_size_) {
            size_t mid = ents.size() / 2;
            std::vector<BranchEntryV1> left(ents.begin(), ents.begin() + mid);
            std::vector<BranchEntryV1> right(ents.begin() + mid + 1, ents.end()); // promote mid sep
            CompositeKey promote = ents[mid].sep_key;
            std::vector<std::uint8_t> lpage(page_size_, 0), rpage(page_size_, 0);
            // leftmost child of right page is the child of promoted sep
            std::uint32_t right_leftmost = ents[mid].child_page;
            build_branch_page_v1(lpage, page_size_, left, leftmost, nullptr, nullptr);
            build_branch_page_v1(rpage, page_size_, right, right_leftmost, nullptr, nullptr);
            if (wal_) {
                auto last = wal_->next_lsn() ? wal_->next_lsn() - 1 : 0;
                reinterpret_cast<ods::PageHeader*>(lpage.data())->scn = last;
                reinterpret_cast<ods::PageHeader*>(rpage.data())->scn = last;
            }
            auto* lhdr = reinterpret_cast<ods::PageHeader*>(lpage.data());
            lhdr->type = static_cast<std::uint16_t>(ods::PageType::IndexBranch);
            std::uint32_t right_no = alloc_page();
            auto* rhdr = reinterpret_cast<ods::PageHeader*>(rpage.data());
            rhdr->type = static_cast<std::uint16_t>(ods::PageType::IndexBranch);
            // write left into original page
            write_page(page_no, lpage);
            write_page(right_no, rpage);
            return InsertUp{true, promote, right_no};
        } else {
            build_branch_page_v1(page, page_size_, ents, leftmost, nullptr, nullptr);
            if (wal_) {
                auto last = wal_->next_lsn() ? wal_->next_lsn() - 1 : 0;
                reinterpret_cast<ods::PageHeader*>(page.data())->scn = last;
            }
            write_page(page_no, page);
            return InsertUp{};
        }
    }

    BTreeV1::InsertUp BTreeV1::insert_recursive(std::uint32_t page_no, bool /*is_leaf_hint*/,
                                                const CompositeKey& key, std::uint64_t row_id,
                                                const std::string& payload, std::string& err)
    {
        // detect node type from header
        std::vector<std::uint8_t> header(page_size_, 0);
        // Latch coupling: lock shared, verify type, upgrade when splitting
        SharedPageGuard gS(&latches_, page_no);
        read_page(page_no, header);
        auto* ph = reinterpret_cast<ods::PageHeader*>(header.data());
        bool is_leaf = ph->type == static_cast<std::uint16_t>(ods::PageType::IndexLeaf);
        if (is_leaf) {
            // Upgrade to exclusive for leaf modification
            gS.release();
            ExclusivePageGuard gX(&latches_, page_no);
            (void)gX; // scope keep
            return insert_into_leaf(page_no, key, row_id, payload);
        }
        // branch: find child by separator
        std::vector<BranchEntryV1> ents;
        CompositeKey hk;
        std::string pref;
        std::uint32_t leftmost = 0;
        parse_branch_page_v1(header, ents, hk, pref, leftmost);
        std::uint32_t child = 0;
        for (size_t i = 0; i < ents.size(); ++i) {
            if (cmp_key(key, ents[i].sep_key) < 0) {
                child = (i == 0 ? leftmost : ents[i - 1].child_page);
                break;
            }
        }
        if (child == 0)
            child = (ents.empty() ? leftmost : ents.back().child_page);
        // Prefetch child subtree pages ahead (best-effort)
        const auto& cfg = get_engine_config();
        if (cfg.prefetch_horizon_pages) {
            pager_.prefetch({1, child}, cfg.prefetch_horizon_pages);
        }
        // Recurse
        // Release parent before descending; child will manage its own latching in recursion
        gS.release();
        auto up = insert_recursive(child, false, key, row_id, payload, err);
        if (!up.has)
            return InsertUp{};
        // Insert promoted sep into this branch
        // Reacquire exclusive on this branch to modify
        ExclusivePageGuard gX(&latches_, page_no);
        (void)gX;
        auto r = insert_into_branch(page_no, up.sep, up.right_child);
        return r;
    }

    bool BTreeV1::insert(const CompositeKey& key, std::uint64_t row_id, const std::string& payload,
                         std::string& err)
    {
        if (root_page_ == 0)
            create_empty();
        auto up = insert_recursive(root_page_, root_is_leaf_, key, row_id, payload, err);
        if (!up.has)
            return true;
        // root split: create new root branch
        std::uint32_t new_root = alloc_page();
        std::vector<std::uint8_t> rpage(page_size_, 0);
        // build entries first
        std::vector<BranchEntryV1> ents;
        ents.push_back(BranchEntryV1{up.sep, up.right_child});
        build_branch_page_v1(rpage, page_size_, ents, root_page_, nullptr, nullptr);
        auto* rh = reinterpret_cast<ods::PageHeader*>(rpage.data());
        rh->type = static_cast<std::uint16_t>(ods::PageType::IndexBranch);
        if (wal_) {
            auto lsn = wal_->append_root_update(new_root);
            reinterpret_cast<ods::PageHeader*>(rpage.data())->scn = lsn;
        }
        write_page(new_root, rpage);
        root_page_ = new_root;
        root_is_leaf_ = false;
        return true;
    }

    std::size_t BTreeV1::erase_equal(const CompositeKey& key)
    {
        // Descend path
        if (root_page_ == 0)
            return 0;
        struct Frame {
            std::uint32_t page;
            bool is_leaf;
            std::uint32_t child_index;
        };
        std::vector<Frame> path;
        std::uint32_t page_no = root_page_;
        for (;;) {
            std::vector<std::uint8_t> page(page_size_, 0);
            read_page(page_no, page);
            auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
            if (ph->type == static_cast<std::uint16_t>(ods::PageType::IndexLeaf)) {
                path.push_back({page_no, true, 0});
                break;
            }
            std::vector<BranchEntryV1> ents;
            CompositeKey hk;
            std::string pref;
            std::uint32_t leftmost = 0;
            parse_branch_page_v1(page, ents, hk, pref, leftmost);
            std::uint32_t child = 0;
            std::uint32_t child_index = 0;
            for (size_t i = 0; i < ents.size(); ++i) {
                if (cmp_key(key, ents[i].sep_key) < 0) {
                    child = (i == 0 ? leftmost : ents[i - 1].child_page);
                    child_index = static_cast<std::uint32_t>(i);
                    break;
                }
            }
            if (child == 0) {
                child = (ents.empty() ? leftmost : ents.back().child_page);
                child_index = static_cast<std::uint32_t>(ents.size());
            }
            path.push_back({page_no, false, child_index});
            page_no = child;
        }
        // Erase from leaf
        std::vector<std::uint8_t> lpage(page_size_, 0);
        SharedPageGuard lg(&latches_, page_no);
        read_page(page_no, lpage);
        std::vector<LeafRecordV1> recs;
        CompositeKey hk;
        std::string pref;
        parse_leaf_page_v1(lpage, recs, hk, pref);
        auto old = recs.size();
        recs.erase(std::remove_if(recs.begin(), recs.end(),
                                  [&](const LeafRecordV1& r) { return cmp_key(r.key, key) == 0; }),
                   recs.end());
        std::size_t removed = old - recs.size();
        auto* phl = reinterpret_cast<ods::PageHeader*>(lpage.data());
        std::uint32_t prev_leaf = phl->prev, next_leaf = phl->next;
        if (removed) {
            // Emit delete WAL
            if (wal_) {
                std::vector<std::uint8_t> key_enc;
                detail::encode_key(key, key_enc);
                auto lsn = wal_->append_delete(key_enc);
                lg.release();
                ExclusivePageGuard lx(&latches_, page_no);
                (void)lx;
                build_leaf_page_v1(lpage, page_size_, recs, nullptr, nullptr);
                phl = reinterpret_cast<ods::PageHeader*>(lpage.data());
                phl->scn = lsn;
                phl->prev = prev_leaf;
                phl->next = next_leaf;
                write_page(page_no, lpage);
            } else {
                lg.release();
                ExclusivePageGuard lx(&latches_, page_no);
                (void)lx;
                build_leaf_page_v1(lpage, page_size_, recs, nullptr, nullptr);
                phl = reinterpret_cast<ods::PageHeader*>(lpage.data());
                phl->prev = prev_leaf;
                phl->next = next_leaf;
                write_page(page_no, lpage);
            }
        }
        // Rebalance if underflow (heuristic: less than 2 records and not root)
        if (removed && !path.empty() && !(path.size() == 1 && root_is_leaf_) && recs.size() < 2) {
            // Parent frame is last non-leaf in path
            Frame parent = path[path.size() - 2];
            std::vector<std::uint8_t> ppage(page_size_, 0);
            read_page(parent.page, ppage);
            std::vector<BranchEntryV1> ents;
            CompositeKey hk2;
            std::string pref2;
            std::uint32_t leftmost = 0;
            parse_branch_page_v1(ppage, ents, hk2, pref2, leftmost);
            // Compute siblings
            std::uint32_t left_sib = 0, right_sib = 0;
            std::size_t sep_index = parent.child_index;
            if (sep_index == 0) {
                left_sib = 0;
                right_sib = ents.empty() ? 0 : ents[0].child_page;
            } else if (sep_index >= ents.size()) {
                left_sib =
                    (sep_index == 0 ? 0
                                    : (sep_index == 1 ? leftmost : ents[sep_index - 2].child_page));
                right_sib = 0;
            } else {
                left_sib = (sep_index == 1 ? leftmost : ents[sep_index - 2].child_page);
                right_sib = ents[sep_index].child_page;
            }
            // Try borrow from right sibling
            if (right_sib) {
                std::vector<std::uint8_t> rpage(page_size_, 0);
                read_page(right_sib, rpage);
                std::vector<LeafRecordV1> rrecs;
                CompositeKey hk3;
                std::string pref3;
                parse_leaf_page_v1(rpage, rrecs, hk3, pref3);
                if (rrecs.size() > 2) {
                    // move first from right to leaf
                    LeafRecordV1 moved = rrecs.front();
                    rrecs.erase(rrecs.begin());
                    recs.push_back(moved);
                    std::sort(recs.begin(), recs.end(),
                              [&](const LeafRecordV1& a, const LeafRecordV1& b) {
                                  return cmp_key(a.key, b.key) < 0;
                              });
                    // rewrite both pages
                    build_leaf_page_v1(lpage, page_size_, recs, nullptr, nullptr);
                    auto* hl = reinterpret_cast<ods::PageHeader*>(lpage.data());
                    hl->prev = prev_leaf;
                    hl->next = next_leaf;
                    write_page(page_no, lpage);
                    build_leaf_page_v1(rpage, page_size_, rrecs, nullptr, nullptr);
                    auto* hr = reinterpret_cast<ods::PageHeader*>(rpage.data());
                    auto old_prev = hr->prev;
                    auto old_next = hr->next;
                    hr->prev = page_no;
                    hr->next = old_next;
                    write_page(right_sib, rpage);
                    // update parent separator to new first key of right
                    ents[sep_index].sep_key = rrecs.front().key;
                    build_branch_page_v1(ppage, page_size_, ents, leftmost, nullptr, nullptr);
                    write_page(parent.page, ppage);
                    return removed;
                }
            }
            // Try borrow from left sibling
            if (left_sib) {
                std::vector<std::uint8_t> rpage(page_size_, 0);
                read_page(left_sib, rpage);
                std::vector<LeafRecordV1> lrecsib;
                CompositeKey hk3;
                std::string pref3;
                parse_leaf_page_v1(rpage, lrecsib, hk3, pref3);
                if (lrecsib.size() > 2) {
                    LeafRecordV1 moved = lrecsib.back();
                    lrecsib.pop_back();
                    recs.push_back(moved);
                    std::sort(recs.begin(), recs.end(),
                              [&](const LeafRecordV1& a, const LeafRecordV1& b) {
                                  return cmp_key(a.key, b.key) < 0;
                              });
                    build_leaf_page_v1(lpage, page_size_, recs, nullptr, nullptr);
                    auto* hl = reinterpret_cast<ods::PageHeader*>(lpage.data());
                    hl->prev = prev_leaf;
                    hl->next = next_leaf;
                    write_page(page_no, lpage);
                    build_leaf_page_v1(rpage, page_size_, lrecsib, nullptr, nullptr);
                    auto* hr = reinterpret_cast<ods::PageHeader*>(rpage.data());
                    auto old_prev = hr->prev;
                    auto old_next = hr->next;
                    hr->prev = old_prev;
                    hr->next = page_no;
                    write_page(left_sib, rpage);
                    // update parent separator between left and leaf to new first key of leaf
                    std::size_t idx = sep_index == 0 ? 0 : sep_index - 1;
                    ents[idx].sep_key = recs.front().key;
                    build_branch_page_v1(ppage, page_size_, ents, leftmost, nullptr, nullptr);
                    write_page(parent.page, ppage);
                    return removed;
                }
            }
            // Merge: prefer merge leaf into right sibling if exists, else left into leaf
            if (right_sib) {
                std::vector<std::uint8_t> rpage(page_size_, 0);
                read_page(right_sib, rpage);
                std::vector<LeafRecordV1> rrecs;
                CompositeKey hk3;
                std::string pref3;
                parse_leaf_page_v1(rpage, rrecs, hk3, pref3);
                // append all leaf records into right and rewrite
                rrecs.insert(rrecs.begin(), recs.begin(), recs.end());
                std::sort(rrecs.begin(), rrecs.end(),
                          [&](const LeafRecordV1& a, const LeafRecordV1& b) {
                              return cmp_key(a.key, b.key) < 0;
                          });
                auto* hr = reinterpret_cast<ods::PageHeader*>(rpage.data());
                std::uint32_t rprev = hr->prev, rnext = hr->next;
                build_leaf_page_v1(rpage, page_size_, rrecs, nullptr, nullptr);
                hr = reinterpret_cast<ods::PageHeader*>(rpage.data());
                hr->prev = prev_leaf;
                hr->next = rnext;
                write_page(right_sib, rpage);
                // fix neighbors skipping the removed leaf
                if (prev_leaf) {
                    std::vector<std::uint8_t> p2(page_size_, 0);
                    read_page(prev_leaf, p2);
                    auto* hp2 = reinterpret_cast<ods::PageHeader*>(p2.data());
                    hp2->next = right_sib;
                    write_page(prev_leaf, p2);
                }
                if (rnext) {
                    std::vector<std::uint8_t> n2(page_size_, 0);
                    read_page(rnext, n2);
                    auto* hn2 = reinterpret_cast<ods::PageHeader*>(n2.data());
                    hn2->prev = right_sib;
                    write_page(rnext, n2);
                }
                // remove separator at sep_index from parent
                ents.erase(ents.begin() + sep_index);
                build_branch_page_v1(ppage, page_size_, ents, leftmost, nullptr, nullptr);
                write_page(parent.page, ppage);
                // If parent became empty and is root, collapse
                if (parent.page == root_page_) {
                    if (ents.empty()) {
                        root_page_ = right_sib;
                        root_is_leaf_ = true;
                    }
                }
                return removed;
            } else if (left_sib) {
                std::vector<std::uint8_t> lsp(page_size_, 0);
                read_page(left_sib, lsp);
                std::vector<LeafRecordV1> lrecsib;
                CompositeKey hk3;
                std::string pref3;
                parse_leaf_page_v1(lsp, lrecsib, hk3, pref3);
                lrecsib.insert(lrecsib.end(), recs.begin(), recs.end());
                std::sort(lrecsib.begin(), lrecsib.end(),
                          [&](const LeafRecordV1& a, const LeafRecordV1& b) {
                              return cmp_key(a.key, b.key) < 0;
                          });
                auto* hl = reinterpret_cast<ods::PageHeader*>(lsp.data());
                std::uint32_t lprev = hl->prev;
                std::uint32_t lnext = hl->next;
                build_leaf_page_v1(lsp, page_size_, lrecsib, nullptr, nullptr);
                hl = reinterpret_cast<ods::PageHeader*>(lsp.data());
                hl->prev = lprev;
                hl->next = next_leaf;
                write_page(left_sib, lsp);
                if (next_leaf) {
                    std::vector<std::uint8_t> n2(page_size_, 0);
                    read_page(next_leaf, n2);
                    auto* hn2 = reinterpret_cast<ods::PageHeader*>(n2.data());
                    hn2->prev = left_sib;
                    write_page(next_leaf, n2);
                }
                // remove separator before this position (sep_index-1)
                std::size_t idx = sep_index == 0 ? 0 : sep_index - 1;
                if (!ents.empty() && idx < ents.size())
                    ents.erase(ents.begin() + idx);
                build_branch_page_v1(ppage, page_size_, ents, leftmost, nullptr, nullptr);
                write_page(parent.page, ppage);
                if (parent.page == root_page_) {
                    if (ents.empty()) {
                        root_page_ = left_sib;
                        root_is_leaf_ = true;
                    }
                }
                return removed;
            }
        }
        return removed;
    }

    void BTreeV1::inorder_keys(std::vector<std::string>& out) const
    {
        out.clear();
        // walk from leftmost leaf following next links
        if (root_page_ == 0)
            return;
        std::uint32_t leaf = root_page_;
        // descend to leftmost through branches
        for (;;) {
            SharedPageGuard gS(&latches_, leaf);
            std::vector<std::uint8_t> page(page_size_, 0);
            read_page(leaf, page);
            auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
            if (ph->type == static_cast<std::uint16_t>(ods::PageType::IndexLeaf))
                break;
            // branch: follow leftmost pointer
            std::vector<BranchEntryV1> ents;
            CompositeKey hk;
            std::string pref;
            std::uint32_t leftmost = 0;
            parse_branch_page_v1(page, ents, hk, pref, leftmost);
            leaf = leftmost;
            if (leaf == 0)
                return; // corrupt guard
            // release branch latch before moving down
            gS.release();
        }
        while (leaf) {
            SharedPageGuard gS(&latches_, leaf);
            std::vector<std::uint8_t> page(page_size_, 0);
            read_page(leaf, page);
            std::vector<LeafRecordV1> recs;
            CompositeKey hk;
            std::string pref;
            parse_leaf_page_v1(page, recs, hk, pref);
            for (auto& r : recs)
                out.push_back(key_to_string(r.key));
            std::uint32_t next = reinterpret_cast<ods::PageHeader*>(page.data())->next;
            // hint next leaf prefetch
            const auto& cfg = get_engine_config();
            if (next && cfg.prefetch_horizon_pages) {
                pager_.prefetch({1, next}, cfg.prefetch_horizon_pages);
            }
            gS.release();
            leaf = next;
        }
    }

} // namespace scratchbird::engine
