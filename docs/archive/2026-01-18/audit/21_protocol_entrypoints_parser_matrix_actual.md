# Protocol Entry Points and Parser Paths (Actual)

Purpose: Quick matrices mapping server protocol entry points to the parser/compiler pipeline and runtime execution path.

Status: static code review snapshot; no runtime execution performed.

## Server protocol listeners (defaults)

| Protocol | Default port | Adapter class | Notes |
| --- | --- | --- | --- |
| Native | 3092 | `scratchbird::protocol::NativeAdapter` | ScratchBird native wire protocol |
| PostgreSQL | 5432 | `scratchbird::protocol::PostgresqlAdapter` | PostgreSQL v3-compatible server protocol |
| MySQL | 3306 | `scratchbird::protocol::MySqlAdapter` | MySQL/MariaDB-compatible server protocol |
| Firebird | 3050 | `scratchbird::protocol::FirebirdAdapter` | Firebird wire protocol |

Sources: `ScratchBird/src/server/service_controller.cpp`, `ScratchBird/src/protocol/adapters/protocol_adapter.cpp`.

## Auto-detect entry point (shared listener)

`ConnectionManager::detectProtocol` uses initial bytes to guess protocol:
- SB magic -> Native
- PostgreSQL startup/SSL request -> PostgreSQL
- MySQL packet framing -> MySQL
- Firebird op_connect -> Firebird
- Default fallback -> PostgreSQL

Source: `ScratchBird/src/network/connection_handler.cpp`.

## Query compilation path per protocol (server-side)

| Protocol | compileQuery implementation | Query compiler | Parser / pipeline | Runtime execution |
| --- | --- | --- | --- | --- |
| Native | `ProtocolAdapter::compileQuery` (base) | `sblr::QueryCompilerV2` | Parser V2 -> SemanticAnalyzerV2 -> BytecodeGeneratorV2 | `sblr::Executor::execute` |
| PostgreSQL | `PostgresqlAdapter::compileQuery` | `sblr::PostgreSQLQueryCompiler` | PostgreSQL parser (direct SBLR) | `sblr::Executor::execute` |
| MySQL | `MySqlAdapter::compileQuery` | `sblr::MySQLQueryCompiler` | MySQL parser (direct SBLR) | `sblr::Executor::execute` |
| Firebird | `FirebirdAdapter::compileQuery` | `sblr::FirebirdQueryCompiler` | Firebird parser -> SemanticAnalyzerV2 -> BytecodeGeneratorV2 | `sblr::Executor::execute` |

Sources: `ScratchBird/src/protocol/adapters/*.cpp`, `ScratchBird/src/sblr/*_query_compiler.cpp`.

## Dialect tagging and schema selection

- `ProtocolAdapter::ensureEngine` sets `ConnectionContext` dialect tag based on protocol: `postgresql`, `mysql`, `firebird`, or `scratchbird`.
- PostgreSQL/MySQL adapters set default schema to `remote.emulated.<dialect>.localhost.<db>`.
- Firebird adapter initializes Firebird system tables and uses the Firebird query compiler (AST v2 pipeline).

Source: `ScratchBird/src/protocol/adapters/protocol_adapter.cpp`, `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp`, `ScratchBird/src/protocol/adapters/mysql_adapter.cpp`, `ScratchBird/src/protocol/adapters/firebird_adapter.cpp`.
