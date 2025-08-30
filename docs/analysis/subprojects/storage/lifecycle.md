[ScratchBird Analysis Documentation](../../index.md) / [Storage](index.md)

### Heap lifecycle: create, open, insert, scan, truncate, drop

How heap relations are created and operated on, with key code anchors.

## Implementation References
- Relation API: `ScratchBird/include/scratchbird/engine/heap_rel.h`
- ODS and Allocator used by relation: `ScratchBird/include/scratchbird/engine/ods.h`, `ScratchBird/src/engine/alloc.cpp`

### Create and open
```27:46:include/scratchbird/engine/heap_rel.h
    class HeapRelation
    {
      public:
        HeapRelation(FileMap fmap, std::uint32_t page_size, std::uint32_t root_page,
                     TupleLayout layout);
        struct RelRoot { /* ... */ };

        static HeapRelation create(FileMap fmap, std::uint32_t page_size, const TupleLayout& layout,
                                   const HeapOptions& opts = {})
        {
            // Allocate pages via Allocator; write HeapRoot and first HeapData
            // ... existing code ...
        }

        static HeapRelation open(FileMap fmap, std::uint32_t page_size, std::uint32_t root_page,
                                 const TupleLayout& layout);
```

```52:81:include/scratchbird/engine/heap_rel.h
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
```

### Insert (non-txn and txn variants)
```97:146:include/scratchbird/engine/heap_rel.h
        InsertResult insert(const std::vector<Value>& values)
        {
            // Ensure data page with space; write tuple and slot; update root
            // ... existing code ...
        }
```

```248:336:include/scratchbird/engine/heap_rel.h
        InsertResult insert_txn(const std::vector<Value>& values, const Transaction& tx)
        {
            // Set created_xid in tuple header and proceed like insert
            // ... existing code ...
        }
```

```220:229:include/scratchbird/engine/heap_rel.h
            std::uint16_t off = all_eight ? HeapPageCodec::write_raw_tuple_aligned(page, bytes, 8)
                                          : HeapPageCodec::write_raw_tuple(page, bytes);
            std::uint16_t slot = HeapPageCodec::push_slot(page, off);
            {
                auto* dph = reinterpret_cast<ods::PageHeader*>(page.data());
                dph->checksum = 0;
                dph->checksum = ods::crc32c(page.data(), page.size());
            }
            fmap_.write_page(data_pgno, page.data());
```

### Scan and visibility
```797:814:include/scratchbird/engine/heap_rel.h
    inline HeapScan HeapRelation::open_scan_visible(const SnapshotRC& snap,
                                    std::function<TxnState(std::uint64_t)> read_state) const
    {
        // Opens iterator with visibility filtering
    }
```

```668:761:include/scratchbird/engine/heap_rel.h
    class HeapScan
    {
      public:
        bool next(std::vector<Value>& out, ods::RowId* rid_out)
        {
            // Slot iteration, overflow reads, visibility filter, decode
            // ... existing code ...
        }
    };
```

```740:753:include/scratchbird/engine/heap_rel.h
                    if (rr_mode_) {
                        if (!is_visible_rr(rr_snap_, th, read_state_))
                            continue;
                    } else {
                        if (!is_visible_rc(snap_, th, read_state_))
                            continue;
                    }
```

### Truncate and drop
```382:414:include/scratchbird/engine/heap_rel.h
        void truncate()
        {
            // Free data/overflow pages and keep/new HeapData
            // ... existing code ...
        }
```

```416:438:include/scratchbird/engine/heap_rel.h
        void drop()
        {
            // Free all pages including root
            // ... existing code ...
        }
```

## Spec Trace
- [REQ-CORE-HEAP-API](../../traceability/spec/requirements.md#req-core-heap-api)
- [REQ-CORE-HEAP-SCAN](../../traceability/spec/requirements.md#req-core-heap-scan)
- [REQ-CORE-SPACE-RECLAIM](../../traceability/spec/requirements.md#req-core-space-reclaim)


## Related
- [Heap: tuple and page format, visibility, codecs](heap.md)
- [Storage](index.md)
- [On-Disk Structures (ODS)](ods.md)
- [Space management and Allocator](space_allocator.md)
