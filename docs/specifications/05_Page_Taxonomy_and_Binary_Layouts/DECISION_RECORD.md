# Section 05 Decision Record

## Current decisions

1. `ondisk.h` is the shared durable page-contract authority.
2. The common page header owns page identity, flags, checksum fields, generation fields, and repair-state fields.
3. Heap-page and index-page family behavior are layered on the common page header rather than replacing it.
4. Integrity is checked over the stored page image; publication order must finalize integrity fields before durable write publication.
5. Compression and encryption are current page-image capabilities, but their framing and support are family/path specific rather than universally embedded in one direct header schema.
6. Reserved emulation page types are authoritative numeric reservations, not proof that every such family is fully implemented.
7. Restart-visible repair markers are page-local evidence only and do not replace MGA, TIP, OIT, OAT, or OST truth.

## Rejected interpretations

- Treating page-local markers as transaction or checkpoint authority.
- Treating reserved page-type enums as proof of full storage-engine ownership.
- Treating derivative compression framing as proof of identical framing in every page family.
