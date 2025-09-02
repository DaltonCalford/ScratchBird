[ScratchBird Analysis Documentation](../../index.md) / [Storage](index.md)

### Heap: tuple and page format, visibility, codecs

Tuple encoding/decoding, heap page layout, and visibility helpers.

## Implementation References
- Header: `ScratchBird/include/scratchbird/engine/heap.h`
- Source: `ScratchBird/src/engine/heap.cpp`
- Relation API: `ScratchBird/include/scratchbird/engine/heap_rel.h`

### Tuple layout and codec
```100:139:include/scratchbird/engine/heap.h
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
```

```78:99:include/scratchbird/engine/heap.h
    inline bool is_visible_rr(const SnapshotRR& snap, const ods::TupleHeader& th,
                              const std::function<TxnState(std::uint64_t)>& read_state)
    {
        // ... existing code ...
    }
```

```124:186:include/scratchbird/engine/heap.h
    class HeapTupleCodec
    {
      public:
        // Encode fixed-width-only tuple (Int64) with nullmap.
        static std::vector<std::uint8_t> encode_tuple(const TupleLayout& layout,
                                                      const std::vector<Value>& values)
        {
            // ... existing code ...
        }

        static bool decode_tuple(
            const TupleLayout& layout, const std::vector<std::uint8_t>& page, std::uint16_t off,
            std::vector<Value>& out,
            const std::function<bool(const ods::OverflowRef&, std::string&)>& overflow_reader = {})
        {
            // ... existing code ...
        }
    };
```

```1:11:src/engine/heap.cpp
#include "scratchbird/engine/heap.h"

#include <cstring>
#include <stdexcept>

namespace scratchbird::engine
{

    // Implementation is header-only for now; this TU exists for build/link consistency.

}
```

```250:337:include/scratchbird/engine/heap.h
        // Encode with overflow handling: write overflow via callback returning new page number
        static std::vector<std::uint8_t> encode_tuple_with_overflow(
            const TupleLayout& layout, const std::vector<Value>& values, std::uint32_t page_size,
            const std::function<std::uint32_t(const std::uint8_t*, std::size_t)>& write_overflow,
            std::uint32_t overflow_threshold_pct = ods::HEAP_OVERFLOW_THRESHOLD_PCT,
            std::uint32_t safety_margin = 128, std::uint64_t created_xid = 0)
        {
            // ... existing code ...
        }
```

### Heap page layout and checks
```340:372:include/scratchbird/engine/heap.h
    struct HeapLayout {
        static std::uint32_t page_header_size();
        static std::uint32_t heap_header_size();
        static std::uint32_t tuples_region_start();
    };

    class HeapPageCodec
    {
      public:
        static void init_heap_data_page(std::vector<std::uint8_t>& page);
        static ods::HeapPageHeader read_heap_hdr(const std::vector<std::uint8_t>& page);
        static void write_heap_hdr(std::vector<std::uint8_t>& page, const ods::HeapPageHeader& h);
        static std::uint32_t free_bytes(const std::vector<std::uint8_t>& page);
        static std::uint16_t write_raw_tuple(std::vector<std::uint8_t>& page,
                                             const std::vector<std::uint8_t>& bytes);
        static std::uint16_t write_raw_tuple_aligned(std::vector<std::uint8_t>& page,
                                                     const std::vector<std::uint8_t>& bytes,
                                                     std::uint16_t align);
        static std::uint16_t push_slot(std::vector<std::uint8_t>& page, std::uint16_t offset);
        static void set_slot_offset(std::vector<std::uint8_t>& page, std::uint16_t slot_index,
                                    std::uint16_t offset);
```

```223:239:include/scratchbird/engine/heap_rel.h
            {
                auto* dph = reinterpret_cast<ods::PageHeader*>(page.data());
                dph->checksum = 0;
                dph->checksum = ods::crc32c(page.data(), page.size());
            }
```

```471:517:include/scratchbird/engine/heap.h
        static bool check_heap_page_invariants(const std::vector<std::uint8_t>& page,
                                               std::string& error)
        {
            // ... existing code ...
        }
    };
```

### Visibility helpers
```42:72:include/scratchbird/engine/heap.h
    inline bool is_visible_rc(const SnapshotRC& snap, const ods::TupleHeader& th,
                              const std::function<TxnState(std::uint64_t)>& read_state)
    {
        // ... existing code ...
    }
```

```78:99:include/scratchbird/engine/heap.h
    inline bool is_visible_rr(const SnapshotRR& snap, const ods::TupleHeader& th,
                              const std::function<TxnState(std::uint64_t)>& read_state)
    {
        // ... existing code ...
    }
```

```742:753:include/scratchbird/engine/heap_rel.h
                    if (rr_mode_) {
                        if (!is_visible_rr(rr_snap_, th, read_state_))
                            continue;
                    } else {
                        if (!is_visible_rc(snap_, th, read_state_))
                            continue;
                    }
```

## Spec Trace
- [REQ-CORE-HEAP-TUPLE-FORMAT](../../traceability/spec/requirements.md#req-core-heap-tuple-format)
- [REQ-CORE-HEAP-API](../../traceability/spec/requirements.md#req-core-heap-api)
- [REQ-CORE-HEAP-SCAN](../../traceability/spec/requirements.md#req-core-heap-scan)


## Related
- [Storage](index.md)
- [Heap lifecycle: create, open, insert, scan, truncate, drop](lifecycle.md)
- [On-Disk Structures (ODS)](ods.md)
- [Space management and Allocator](space_allocator.md)
