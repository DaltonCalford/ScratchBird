# Beta 2 DICOM Metadata And Image Container UDR Model

## Purpose

This document defines the DICOM-oriented UDR family for metadata extraction,
tag validation, and structured imaging-container introspection.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `DICOM` metadata tooling.

## Owning package

- `sb_pkg_dicom_udr`

## Dependencies

This package depends on:

- `sb_pkg_contract_udr`
- `sb_pkg_doc_udr`

## Mandatory surfaces

The package shall provide:

- DICOM container detection
- tag extraction
- selected tag validation
- study/series/image hierarchy projection
- de-identification helper hooks for the admitted subset

## Required routine families

- `sb_dicom.detect(...)`
- `sb_dicom.tags(...)`
- `sb_dicom.validate(...)`
- `sb_dicom.hierarchy(...)`
- `sb_dicom.deidentify_preview(...)`

## Example contract

```sql
select *
from sb_dicom.tags(:dicom_blob);
```

## Operational rules

1. Pixel/image decoding is not required for Beta 2 unless explicitly promoted
   for a bounded subset.
2. Metadata extraction must be bounded and fail closed on malformed input.
3. De-identification helpers must report exactly which tags are affected.

## Explicit exclusions

- full imaging viewer/runtime
- remote PACS integration as a baseline requirement
