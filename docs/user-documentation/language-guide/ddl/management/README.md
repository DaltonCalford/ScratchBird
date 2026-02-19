# DDL Family: Management
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../README.md)
- [DDL README](../README.md)

## Coverage Summary (0.1.0)
| Object | CREATE | ALTER | SHOW | DESCRIBE | DROP | Lifecycle |
|---|---|---|---|---|---|---|
| [DATABASE](database/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [SCHEMA](schema/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [TABLESPACE](tablespace/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |

## Objects
- [DATABASE](database/README.md): Native database lifecycle exists; emulated create path is additionally exposed.
- [SCHEMA](schema/README.md): Schema namespace lifecycle is closed in parser command families.
- [TABLESPACE](tablespace/README.md): Missing explicit SHOW/DESCRIBE command keeps lifecycle partial.

## Naming And Identifier Rules
- [Object Naming And Identifiers](object-naming-and-identifiers.md): UTF-8 identifier limits, case-normalization rules, and naming style guidance.
