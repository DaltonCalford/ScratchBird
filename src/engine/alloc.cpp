#include "scratchbird/engine/alloc.h"

#include "scratchbird/engine/config.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace scratchbird::engine
{

    void Allocator::write_header()
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        ods::PageHeader* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = 0;
        hdr->space_id = 1;
        hdr->type = static_cast<std::uint16_t>(ods::PageType::Header);
        hdr->page_size = page_size_;
        hdr->header_version = 1;
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_->write_page(0, page.data());
    }

    std::uint64_t Allocator::pip_base_for(std::uint32_t page_no) const
    {
        // PIP at page 1 for first region, then every pages_per_pip + 1 (reserve 1 for PIP)
        const std::uint32_t ppp = pages_per_pip();
        if (page_no <= 1 + ppp)
            return 1; // first PIP
        std::uint32_t region = (page_no - 1) / (ppp + 1);
        return static_cast<std::uint64_t>(region) * (ppp + 1) + 1;
    }

    void Allocator::ensure_pip_for(std::uint32_t page_no)
    {
        std::uint64_t pip_base = pip_base_for(page_no);
        // naive: write PIP page if all zeros implies uninitialized
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_->read_page(pip_base, page.data());
        bool zero = true;
        for (auto b : page)
            if (b) {
                zero = false;
                break;
            }
        if (zero) {
            ods::PageHeader* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
            hdr->page_no = static_cast<std::uint32_t>(pip_base);
            hdr->space_id = 1;
            hdr->type = static_cast<std::uint16_t>(ods::PageType::Pip);
            hdr->page_size = page_size_;
            hdr->checksum = 0;
            hdr->checksum = ods::crc32c(page.data(), page.size());
            fmap_->write_page(pip_base, page.data());
        }
    }

    bool Allocator::pip_test(std::uint64_t pip_base_page, std::uint32_t idx)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_->read_page(pip_base_page, page.data());
        // bitmap starts after header reserve (64 bytes)
        std::size_t bit = idx;
        std::size_t byte_off = 64 + (bit / 8);
        std::uint8_t mask = 1u << (bit % 8);
        return (page[byte_off] & mask) != 0;
    }

    void Allocator::pip_set(std::uint64_t pip_base_page, std::uint32_t idx, bool value)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_->read_page(pip_base_page, page.data());
        std::size_t bit = idx;
        std::size_t byte_off = 64 + (bit / 8);
        std::uint8_t mask = 1u << (bit % 8);
        if (value)
            page[byte_off] |= mask;
        else
            page[byte_off] &= ~mask;
        // update checksum
        ods::PageHeader* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_->write_page(pip_base_page, page.data());
    }

    void Allocator::init_new()
    {
        write_header();
        // Initialize first PIP and mark header + PIP used
        ensure_pip_for(1);
        std::uint64_t pip_base = pip_base_for(1);
        // Mark bit 0 for PIP itself, and page 0 (header) is outside bitmap; mark page 1 used for
        // simplicity
        pip_set(pip_base, 0, true);
        // Initialize SpaceCatalog at page 3 (simple fixed location for Phase 2)
        std::vector<std::uint8_t> sc(page_size_, 0);
        ods::PageHeader* sch = reinterpret_cast<ods::PageHeader*>(sc.data());
        sch->page_no = 3;
        sch->space_id = 1;
        sch->type = static_cast<std::uint16_t>(ods::PageType::SpaceCatalog);
        sch->page_size = page_size_;
        ods::SpaceCatalogPayload scp{};
        scp.version = 1;
        scp.space_id = 1;
        scp.page_size = page_size_;
        scp.pip_root_page = static_cast<std::uint32_t>(pip_base);
        scp.tip_root_page = 2; // reserved TIP location
        scp.segments = 1;
        scp.next_extent_id = 1;
        std::memcpy(sc.data() + sizeof(ods::PageHeader), &scp, sizeof scp);
        sch->checksum = 0;
        sch->checksum = ods::crc32c(sc.data(), sc.size());
        fmap_->write_page(3, sc.data());
    }

    std::uint32_t Allocator::allocate_free_page()
    {
        const std::uint32_t ppp = pages_per_pip();
        // Scan regions sequentially
        for (std::uint32_t region = 0;; ++region) {
            std::uint64_t pip_base = static_cast<std::uint64_t>(region) * (ppp + 1) + 1;
            ensure_pip_for(static_cast<std::uint32_t>(pip_base));
            for (std::uint32_t i = 0; i < ppp; ++i) {
                if (!pip_test(pip_base, i)) {
                    pip_set(pip_base, i, true);
                    // Optional prefetch hint for sequential allocations
                    const auto& cfg = get_engine_config();
                    if (cfg.prefetch_on_alloc && cfg.prefetch_horizon_pages) {
                        std::uint64_t first = pip_base + 1 + i;
                        std::uint64_t off = first * static_cast<std::uint64_t>(page_size_);
                        std::size_t len =
                            static_cast<std::size_t>(cfg.prefetch_horizon_pages) * page_size_;
                        // best-effort: open seg and issue fadvise
                        auto [segIdx, segOff] = fmap_->map(first);
                        (void)segIdx;
                        (void)segOff; // segOff equals off within segment
                        if (!fmap_->segments().empty()) {
                            FileManager::prefetch_willneed(
                                fmap_->segments()[segIdx].handle,
                                off - (segIdx * static_cast<std::uint64_t>(page_size_) *
                                       fmap_->map(0).second),
                                len);
                        }
                    }
                    // physical page number is pip_base + 1 + i (since pip occupies one page)
                    return static_cast<std::uint32_t>(pip_base + 1 + i);
                }
            }
        }
    }

    std::uint32_t Allocator::allocate_extent(std::uint32_t count)
    {
        const std::uint32_t ppp = pages_per_pip();
        for (std::uint32_t region = 0;; ++region) {
            std::uint64_t pip_base = static_cast<std::uint64_t>(region) * (ppp + 1) + 1;
            ensure_pip_for(static_cast<std::uint32_t>(pip_base));
            for (std::uint32_t i = 0; i + count <= ppp; ++i) {
                bool free_run = true;
                for (std::uint32_t j = 0; j < count; ++j) {
                    if (pip_test(pip_base, i + j)) {
                        free_run = false;
                        break;
                    }
                }
                if (free_run) {
                    for (std::uint32_t j = 0; j < count; ++j)
                        pip_set(pip_base, i + j, true);
                    return static_cast<std::uint32_t>(pip_base + 1 + i);
                }
            }
        }
    }

    void Allocator::free_page(std::uint32_t page_no)
    {
        std::uint64_t pip_base = pip_base_for(page_no);
        std::uint32_t idx = static_cast<std::uint32_t>((page_no - (pip_base + 1)));
        pip_set(pip_base, idx, false);
    }

    void Allocator::reserve_until(std::uint32_t last_page)
    {
        if (last_page < 2)
            return;
        for (std::uint32_t p = 2; p <= last_page; ++p) {
            std::uint64_t pip_base = pip_base_for(p);
            ensure_pip_for(p);
            std::uint32_t idx = static_cast<std::uint32_t>((p - (pip_base + 1)));
            if (!pip_test(pip_base, idx))
                pip_set(pip_base, idx, true);
        }
    }

} // namespace scratchbird::engine
