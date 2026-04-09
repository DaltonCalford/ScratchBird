# Beta 2 Document Extraction And Metadata Normalization UDR Model

## Purpose

This document defines the document-processing UDR family for MIME detection,
text extraction, metadata extraction, and structured document normalization.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `Apache Tika`.

## Owning package

- `sb_pkg_doc_udr`

## Dependencies

This package depends on:

- `sb_pkg_text_udr`
- `sb_pkg_contract_udr`

## Mandatory surfaces

The package shall provide:

- MIME/type detection
- document metadata extraction
- plain text extraction from the admitted document formats
- structured metadata rowset projection
- document-to-json normalization for the admitted subset
- attachment manifest extraction for container formats where supported

## Required routine families

- `sb_doc.detect_type(...)`
- `sb_doc.extract_text(...)`
- `sb_doc.extract_metadata(...)`
- `sb_doc.extract_json(...)`
- `sb_doc.extract_rows(...)`

## Example contract

```sql
select *
from sb_doc.extract_metadata(:document_blob);
```

## Operational rules

1. Format support shall be explicit and versioned.
2. Binary inputs must be scanned for size, type, and bounded parser admission
   before extraction.
3. Extraction failures shall return structured diagnostics, not partial silent
   truncation.

## Explicit exclusions

- OCR as a baseline requirement
- unrestricted macro/script execution in document formats
- arbitrary remote document fetch
