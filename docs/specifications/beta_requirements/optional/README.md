# Optional Beta Specifications

This directory holds optional beta-phase engine features that are not required for the initial beta release but are approved for implementation during beta.

## Index

- [STORAGE_ENCODING_OPTIMIZATIONS.md](STORAGE_ENCODING_OPTIMIZATIONS.md) - Varlen header v2, per-column TOAST, packed NUMERIC
- [TABLESPACE_SHRINK_COMPACTION.md](TABLESPACE_SHRINK_COMPACTION.md) - Tablespace shrink/compaction (MGA-safe)

## Notes

- These specs should not override Alpha or core storage specs unless the table storage_format is explicitly set to v2.
- Add new optional specs here and link them in this index.
