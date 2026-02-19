# DDL Family: Integration
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../README.md)
- [DDL README](../README.md)

## Coverage Summary (0.1.0)
| Object | CREATE | ALTER | SHOW | DESCRIBE | DROP | Lifecycle |
|---|---|---|---|---|---|---|
| [REPLICATION CHANNEL](replication-channel/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |
| [PUBLICATION](publication/README.md) | Supported | Not available | Not available | Not available | Supported | Partial command lifecycle |
| [SUBSCRIPTION](subscription/README.md) | Supported | Not available | Not available | Not available | Supported | Partial command lifecycle |
| [CDC TABLE](cdc-table/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |
| [DATABASE CONNECTION](database-connection/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |
| [FOREIGN DATA WRAPPER](foreign-data-wrapper/README.md) | Supported | Not available | Not available | Not available | Not available | Partial command lifecycle |
| [FOREIGN SERVER](foreign-server/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |
| [FOREIGN TABLE](foreign-table/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |
| [USER MAPPING](user-mapping/README.md) | Supported | Not available | Not available | Not available | Supported | Partial command lifecycle |
| [SYNONYM](synonym/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |
| [EXTENSION](extension/README.md) | Supported | Supported | Not available | Not available | Supported | Partial command lifecycle |

## Objects
- [REPLICATION CHANNEL](replication-channel/README.md): SHOW/DESCRIBE missing keeps lifecycle partial.
- [PUBLICATION](publication/README.md): Create/drop only in parser command surface.
- [SUBSCRIPTION](subscription/README.md): Create/drop only in parser command surface.
- [CDC TABLE](cdc-table/README.md): SHOW/DESCRIBE missing keeps lifecycle partial.
- [DATABASE CONNECTION](database-connection/README.md): External connection object has create/alter/drop surfaces but no explicit show/describe command.
- [FOREIGN DATA WRAPPER](foreign-data-wrapper/README.md): Create-only parser surface in 0.1.0.
- [FOREIGN SERVER](foreign-server/README.md): SHOW/DESCRIBE missing keeps lifecycle partial.
- [FOREIGN TABLE](foreign-table/README.md): SHOW/DESCRIBE missing keeps lifecycle partial.
- [USER MAPPING](user-mapping/README.md): Create/drop only in parser command surface.
- [SYNONYM](synonym/README.md): SHOW/DESCRIBE missing keeps lifecycle partial.
- [EXTENSION](extension/README.md): SHOW/DESCRIBE missing keeps lifecycle partial.
