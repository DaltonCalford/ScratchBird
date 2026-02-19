# Native Parser Guide

- Version: `0.1.0`
- Baseline date: `2026-02-19`

## Primary Reference

- Full language reference: `Language-Reference.md`

## Coverage Highlights

- Core DDL/DML/transaction/session parse surfaces
- Native index-management extensions
- Native measurement/schedule extensions
- Deterministic rejection behavior for invalid extension forms

## Validation Sources

- `tests/unit/test_parser_v3_native_extension_surface.cpp`
- `tests/unit/test_parser_v3_index_management.cpp`
- `tests/unit/test_parser_v3_udr_compile_emitter_contract.cpp`
