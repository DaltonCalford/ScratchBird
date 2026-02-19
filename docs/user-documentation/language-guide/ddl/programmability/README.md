# DDL Family: Programmability
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../README.md)
- [DDL README](../README.md)

## Coverage Summary (0.1.0)
| Object | CREATE | ALTER | SHOW | DESCRIBE | DROP | Lifecycle |
|---|---|---|---|---|---|---|
| [FUNCTION](function/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [PROCEDURE](procedure/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [TRIGGER](trigger/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [PACKAGE](package/README.md) | Supported | Supported | Supported | Not available | Supported | Complete command lifecycle |
| [EXCEPTION](exception/README.md) | Supported | Not available | Not available | Not available | Supported | Partial command lifecycle |
| [UDR](udr/README.md) | Supported | Not available | Not available | Not available | Supported | Partial command lifecycle |

## Objects
- [FUNCTION](function/README.md): Function lifecycle is command-complete through SHOW and DROP.
- [PROCEDURE](procedure/README.md): Procedure lifecycle is command-complete through SHOW and DROP.
- [TRIGGER](trigger/README.md): Trigger lifecycle is command-complete through SHOW and DROP.
- [PACKAGE](package/README.md): Package lifecycle is command-complete through SHOW and DROP.
- [EXCEPTION](exception/README.md): Create/drop only in parser command surface.
- [UDR](udr/README.md): Create/drop only in parser command surface.
