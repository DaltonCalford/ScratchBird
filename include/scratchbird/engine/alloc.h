#ifndef SCRATCHBIRD_ENGINE_ALLOC_H
#define SCRATCHBIRD_ENGINE_ALLOC_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <utility>

namespace scratchbird::engine
{

    class Allocator
    {
      public:
        explicit Allocator(FileMap* fmap, std::uint32_t page_size)
            : fmap_(fmap), page_size_(page_size)
        {
        }

        // Initialize a new database: write header (page 0), first PIP (page 1), mark reserved
        // pages.
        void init_new();

        // Allocate a single free page, honoring PIP boundaries.
        std::uint32_t allocate_free_page();

        // Allocate an extent of n pages (default 8) without crossing a PIP region.
        std::uint32_t allocate_extent(std::uint32_t count = 8);

        // Free a page.
        void free_page(std::uint32_t page_no);

        // Reserve all pages up to and including last_page by marking PIP bits.
        // This is useful to advance allocation to a test region (e.g., page 1000).
        void reserve_until(std::uint32_t last_page);

        FileMap& fmap()
        {
            return *fmap_;
        }
        std::uint32_t page_size() const
        {
            return page_size_;
        }

      private:
        void write_header();
        void ensure_pip_for(std::uint32_t page_no);
        bool pip_test(std::uint64_t pip_base_page, std::uint32_t idx);
        void pip_set(std::uint64_t pip_base_page, std::uint32_t idx, bool value);

        // Compute region parameters
        std::uint32_t pages_per_pip() const
        {
            return ods::pagesPerPIP(page_size_);
        }
        std::uint64_t pip_base_for(std::uint32_t page_no) const;

        FileMap* fmap_;
        std::uint32_t page_size_{4096};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_ALLOC_H
