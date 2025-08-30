### Space management and Allocator

Pointer/Free Page Map (PIP), TIP seed, Space Catalog, and page allocation APIs.

## Implementation References
- Header: `ScratchBird/include/scratchbird/engine/alloc.h`
- Source: `ScratchBird/src/engine/alloc.cpp`
- ODS: `ScratchBird/include/scratchbird/engine/ods.h`

### Allocator API
```10:33:include/scratchbird/engine/alloc.h
namespace scratchbird::engine
{

    class Allocator
    {
      public:
        explicit Allocator(FileMap* fmap, std::uint32_t page_size);

        // Initialize a new database: write header (page 0), first PIP (page 1), mark reserved
        // pages.
        void init_new();

        // Allocate a single free page, honoring PIP boundaries.
        std::uint32_t allocate_free_page();

        // Allocate an extent of n pages (default 8) without crossing a PIP region.
        std::uint32_t allocate_extent(std::uint32_t count = 8);

        // Free a page.
        void free_page(std::uint32_t page_no);
```

### PIP region math and bit operations
```27:41:src/engine/alloc.cpp
    std::uint64_t Allocator::pip_base_for(std::uint32_t page_no) const
    {
        // PIP at page 1 for first region, then every pages_per_pip + 1 (reserve 1 for PIP)
        const std::uint32_t ppp = pages_per_pip();
        if (page_no <= 1 + ppp)
            return 1; // first PIP
        std::uint32_t region = (page_no - 1) / (ppp + 1);
        return static_cast<std::uint64_t>(region) * (ppp + 1) + 1;
    }
```

```61:88:src/engine/alloc.cpp
    bool Allocator::pip_test(std::uint64_t pip_base_page, std::uint32_t idx)
    {
        // bitmap starts after header reserve (64 bytes)
        std::size_t bit = idx;
        std::size_t byte_off = 64 + (bit / 8);
        std::uint8_t mask = 1u << (bit % 8);
        return (page[byte_off] & mask) != 0;
    }

    void Allocator::pip_set(std::uint64_t pip_base_page, std::uint32_t idx, bool value)
    {
        // ... existing code ...
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_->write_page(pip_base_page, page.data());
    }
```

```99:118:src/engine/alloc.cpp
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
        // ... existing code ...
        fmap_->write_page(3, sc.data());
    }
```

### Allocation and reclamation
```120:151:src/engine/alloc.cpp
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
                    // ... existing code ...
                    return static_cast<std::uint32_t>(pip_base + 1 + i);
                }
            }
        }
    }
```

```179:185:src/engine/alloc.cpp
    void Allocator::free_page(std::uint32_t page_no)
    {
        std::uint64_t pip_base = pip_base_for(page_no);
        std::uint32_t idx = static_cast<std::uint32_t>((page_no - (pip_base + 1)));
        pip_set(pip_base, idx, false);
    }
```

```186:197:src/engine/alloc.cpp
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
```

### ODS helpers used by Allocator
```29:44:src/engine/ods.cpp
    std::uint32_t bytesBitPIP(std::uint32_t page_size)
    {
        // Header occupies first 64 bytes (conservative); rest is bitmap
        const std::uint32_t hdrReserve = 64;
        if (page_size <= hdrReserve)
            return 0;
        return page_size - hdrReserve;
    }

    std::uint32_t pagesPerPIP(std::uint32_t page_size)
    {
        const std::uint32_t bytes = bytesBitPIP(page_size);
        return bytes * 8u; // 1 bit per page
    }
```

## Spec Trace
- [REQ-CORE-SPACE-PIP](../../traceability/spec/requirements.md#req-core-space-pip)
- [REQ-CORE-SPACE-TIP-SEED](../../traceability/spec/requirements.md#req-core-space-tip-seed)
- [REQ-CORE-SPACE-CATALOG](../../traceability/spec/requirements.md#req-core-space-catalog)
- [REQ-CORE-SPACE-ALLOCATOR](../../traceability/spec/requirements.md#req-core-space-allocator)
- [REQ-CORE-SPACE-RECLAIM](../../traceability/spec/requirements.md#req-core-space-reclaim)

