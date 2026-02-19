# DDL Family: Data Storage
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../README.md)
- [DDL README](../README.md)

## Coverage Summary (0.1.0)
| Object | CREATE | ALTER | SHOW | DESCRIBE | DROP | Lifecycle |
|---|---|---|---|---|---|---|
| [TABLE](table/README.md) | Supported | Supported | Supported | Supported | Supported | Complete command lifecycle |
| [VIEW](view/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [SEQUENCE OR GENERATOR](sequence-generator/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [DOMAIN](domain/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [TYPE](type/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |
| [INDEX](index/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [SEARCH INDEX](search-index/README.md) | Supported | Partial | Not available | Not available | Supported | Partial command lifecycle |
| [VECTOR INDEX](vector-index/README.md) | Supported | Partial | Not available | Not available | Supported | Partial command lifecycle |
| [MEASUREMENT](measurement/README.md) | Supported | Supported | Not available | Not available | Not available | Partial command lifecycle |
| [ACCESS METHOD](access-method/README.md) | Supported | Not available | Not available | Not available | Not available | Partial command lifecycle |
| [STATISTICS](statistics-object/README.md) | Supported | Not available | Not available | Not available | Not available | Partial command lifecycle |
| [TRANSFORM](transform/README.md) | Supported | Not available | Not available | Not available | Not available | Partial command lifecycle |

## Objects
- [TABLE](table/README.md): Table lifecycle is command-complete including DESCRIBE and TRUNCATE companion flow.
- [VIEW](view/README.md): View lifecycle is complete through SHOW and DROP paths.
- [SEQUENCE OR GENERATOR](sequence-generator/README.md): Sequence/generator lifecycle is command-complete through SHOW and DROP.
- [DOMAIN](domain/README.md): Domain lifecycle is command-complete; advanced domain options are available in CREATE DOMAIN grammar.
- [TYPE](type/README.md): Missing SHOW/DESCRIBE keeps lifecycle partial.
- [INDEX](index/README.md): Classic index lifecycle is command-complete via SHOW INDEX surfaces.
- [SEARCH INDEX](search-index/README.md): ALTER supports only REBUILD in 0.1.0; SHOW/DESCRIBE are missing.
- [VECTOR INDEX](vector-index/README.md): ALTER supports only REBUILD in 0.1.0; SHOW/DESCRIBE are missing.
- [MEASUREMENT](measurement/README.md): No DROP/SHOW commands keep lifecycle partial.
- [ACCESS METHOD](access-method/README.md): Create-only parser surface in 0.1.0.
- [STATISTICS](statistics-object/README.md): Create-only parser surface in 0.1.0.
- [TRANSFORM](transform/README.md): Create-only parser surface in 0.1.0.
