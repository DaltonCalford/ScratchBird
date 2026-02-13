# Collation Runtime Format (V3)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

Date: 2026-02-07
Status: Normative

This document defines the **runtime collation weight format** used by
ScratchBird. It is aligned with Firebird’s collation driver model
(`firebird/src/common/intlobj_new.h`) and provides a deterministic, serialized
format for collation weights used by the executor.

## 1. Goals
- Provide a stable binary format for collation weight tables.
- Allow collation lookup without external dependencies.
- Support Firebird and MySQL single-byte collations plus UCA-based collations.

## 2. File Format: `SBCL`

All collation weight files are stored as `SBCL` binary blobs.

### 2.1 Header
```
struct SBCL_Header {
  char     magic[4];      // "SBCL"
  uint16   version;       // 1
  uint16   flags;         // bitset: 1=UCA, 2=SINGLE_BYTE, 4=CASE_INSENSITIVE,
                          // 8=ACCENT_INSENSITIVE, 16=HAS_CONTRACTIONS,
                          // 32=HAS_EXPANSIONS
  uint16   charset_id;    // catalog charset id
  uint16   collation_id;  // catalog collation id
  uint8    levels;        // number of weight levels (1-4)
  uint8    canonical_width; // bytes per canonical char (Firebird texttype)
  uint16   reserved;      // zero
  uint32   record_count;  // number of weight records
  uint32   offset_records;
  uint32   offset_contractions; // 0 if none
  uint32   offset_expansions;   // 0 if none
  uint32   offset_casefold;     // 0 if none
};
```

### 2.2 Weight Record (Single-Byte + UCA)

Records are sorted by `codepoint` ascending.

```
struct SBCL_Record {
  uint32 codepoint;     // Unicode codepoint
  uint16 w1;            // primary weight
  uint16 w2;            // secondary weight
  uint16 w3;            // tertiary weight
  uint16 w4;            // quaternary weight (0 if not used)
};
```

- For single-byte collations, `record_count` MUST be 256 and codepoints MUST be
  0..255.
- For UCA collations, records cover all codepoints referenced in the tailoring.

### 2.3 Contractions Table

Contractions define multi-codepoint sequences with a single weight.

```
struct SBCL_ContractionHeader {
  uint32 count;
};

struct SBCL_ContractionEntry {
  uint16 length;        // number of codepoints
  uint32 codepoints[length];
  uint16 w1, w2, w3, w4;
};
```

Matching rule: **longest prefix wins**.

### 2.4 Expansions Table

Expansions define a single codepoint expanded into multiple weights.

```
struct SBCL_ExpansionHeader {
  uint32 count;
};

struct SBCL_ExpansionEntry {
  uint32 codepoint;
  uint16 weight_count;
  uint16 weights[weight_count][levels];
};
```

### 2.5 Casefold Table (Optional)

```
struct SBCL_CasefoldHeader { uint32 count; };
struct SBCL_CasefoldEntry  { uint32 src; uint32 dst; };
```

If `CASE_INSENSITIVE` is set, casefold rules MUST be applied before
collation key generation.

## 3. Collation Key Generation (Normative)

1. Normalize string to Unicode NFC.
2. Apply casefold (if `CASE_INSENSITIVE`).
3. For each codepoint:
   - If contraction matches, emit its weights.
   - Else if expansion matches, emit expanded weights.
   - Else emit weights from SBCL_Record.
4. Build sort key by concatenating weight levels:
   - Primary weights, then 0x0000 separator,
   - Secondary weights, then 0x0000 separator,
   - Tertiary weights, then 0x0000 separator,
   - Quaternary weights (optional).

This mirrors Firebird’s `texttype_fn_string_to_key` and `texttype_fn_compare`
pipeline.

## 4. Loader Output Requirements

The collation loader MUST:
- Generate SBCL files for each collation.
- Store SBCL blob IDs in `sys.collations` catalog rows.
- Ensure `collation_name`, `charset_name`, and flags match the SBCL header.

## 5. References
- Firebird collation driver interface: `firebird/src/common/intlobj_new.h`.
- ScratchBird loader contract: `COLLATION_TAILORING_LOADER_SPEC.md`.

