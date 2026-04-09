# Dependencies: TOAST and LOB Storage

## Purpose

This file defines the authoritative dependency contract for section `11`.

## Primary runtime dependencies

`toast.h` and `toast.cpp` own TOAST table creation, pointer materialization, chunk writes, detoast reads, delete behavior, and retire behavior.

`toast_visibility.h` and `toast_visibility.cpp` own TIP-aware chunk lifecycle and reclaim classification.

`heap_page.h` and `heap_page.cpp` own heap tuple TOAST-pointer flagging and heap-page insert, read, update, and delete integration for TOAST values.

`storage_engine.h` and `storage_engine.cpp` own TOAST manager orchestration, detoast use during adjacent storage work, and deferred old-value cleanup routing.

`oversized_value_lifecycle.cpp` owns the shared lifecycle vocabulary for `heap_toast` and `hash_overflow`.

`heap_toast_lob_diagnostics.cpp` owns heap, TOAST, and LOB diagnostic issue generation and chunk-sequence validation.

## Cross-section boundary map

Section `02` owns filespace ownership and relocation boundaries. Section `11` shall not claim broader relocation semantics than section `02` authorizes.

Section `05` owns general page, pointer, and integrity layout rules. Section `11` shall not duplicate or weaken those rules.

Section `08` owns MGA visibility, transaction-state, and restart semantics. Section `11` shall not redefine them.

Section `10` owns reclaim ordering and physical cleanup ordering. Section `11` shall not define an independent reclaim scheduler.

Section `18` owns BTREE index behavior used by the `(chunk_id, chunk_seq)` lookup path. Section `11` shall not redefine index semantics.

## Downstream rule

Any future standalone LOB subsystem shall declare its own dependency graph explicitly. It shall not be inferred from the current TOAST-first dependency map.
