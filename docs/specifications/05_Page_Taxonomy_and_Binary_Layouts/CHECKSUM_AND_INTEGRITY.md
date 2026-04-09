# Checksum and Integrity

Status: current_authority

## 1. Current checksum authority

The current page-integrity contract is defined by the checksum helpers in
`ondisk.h` plus the page-write paths that publish page images.

ScratchBird uses CRC32C-based page-image validation.
This is page-image integrity and classification state, not WAL authority.

## 2. Legacy checksum algorithm

For legacy headers, `calculateLegacyPageChecksum(page, page_size)`:

1. starts CRC with `0xFFFFFFFF`
2. excludes bytes `0x0C..0x0F` from the page image
3. CRCs the remaining bytes
4. XORs the final CRC with `0xFFFFFFFF`

If `page_size < 0x10`, the helper CRCs the available bytes without the normal
field exclusion split.

## 3. Canonical header checksum algorithm

For canonical headers, `calculatePageHeaderChecksum(page, header_bytes)`:

1. requires `header_bytes >= 106`
2. starts CRC with `0xFFFFFFFF`
3. CRCs bytes `0x00..0x0F`
4. skips bytes `0x10..0x13` only
5. resumes CRC from byte `0x14` through the end of the header
6. XORs the final CRC with `0xFFFFFFFF`

This means the header checksum excludes only the `header_checksum` field itself.

## 4. Canonical payload checksum algorithm

For canonical headers, `calculatePagePayloadChecksum(page, page_size, header_bytes)`:

1. requires `header_bytes > 0`
2. requires `header_bytes <= page_size`
3. starts CRC with `0xFFFFFFFF`
4. CRCs bytes from `page + header_bytes` through the end of the page
5. XORs the final CRC with `0xFFFFFFFF`

This payload CRC is stored in `payload_checksum` and is also the current
compatibility alias `checksum`.

## 5. Shared page checksum selector

`calculatePageChecksum(page, page_size)` behaves as:

1. if header is canonical and `header_bytes` is legal:
   - return canonical payload checksum
2. otherwise:
   - return legacy page checksum

So the current shared `calculatePageChecksum` API is payload-oriented for
canonical pages and legacy whole-page-oriented for legacy pages.

## 6. Validation algorithm

`validatePageChecksum(page, page_size)` behaves as:

1. if `PAGE_FLAG_CHECKSUM_VALID` is not set:
   - return success without checksum refusal
2. if header is canonical and `header_bytes` is legal:
   - recompute payload checksum and compare to `payload_checksum`
   - if `header_checksum != 0`, recompute header checksum and compare to
     `header_checksum`
   - accept only if all required comparisons succeed
3. otherwise:
   - recompute legacy page checksum and compare to the legacy checksum field

## 7. Integrity and publication rules

The engine must:

1. finalize checksum state before durable page publication
2. validate page-integrity state before accepting a page as legal durable input
3. keep corruption classification separate from ordinary valid pages
4. reject illegal header-generation or repair-state combinations even when CRC
   matches

## 8. Negative requirements

This file does not authorize:

1. skipping checksum validation when `PAGE_FLAG_CHECKSUM_VALID` requires it
2. treating checksum success as proof that family-local layout is valid
3. deriving commit truth from page checksum state
4. treating checksum state as WAL or log-sequence authority
