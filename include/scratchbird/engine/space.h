#ifndef SCRATCHBIRD_ENGINE_SPACE_H
#define SCRATCHBIRD_ENGINE_SPACE_H

#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/txn.h"

#include <cstdint>
#include <string>

namespace scratchbird::engine
{

    // Create a new tablespace (space_id implied as 1 for standalone files) in the given
    // directory/base. Initializes Header, first PIP, SpaceCatalog, and TIP seed.
    inline void space_create(const std::string& dir, const std::string& base,
                             std::uint32_t page_size, std::uint64_t pages_per_segment,
                             const FileOptions& opts = {})
    {
        FileMap::Layout layout{};
        layout.page_size = page_size;
        layout.pages_per_segment = pages_per_segment;
        layout.options = opts;
        // Allocator init
        FileMap fmap(layout);
        fmap.set_base_path(dir, base);
        Allocator alloc(&fmap, page_size);
        alloc.init_new();
        // TIP seed using a fresh handle to avoid aliasing
        FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);
        TransactionManager tm(std::move(fmap2), page_size);
        tm.init_seed();
    }

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_SPACE_H
