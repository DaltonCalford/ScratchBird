# XOS-047 Cross-OS Listener + DDL/DML Smoke
Last-Modified: 2026-02-22

## Implemented
- Cross-OS runtime smoke includes:
  - Listener front-door lifecycle and direct listener matrix launch
  - Native parser DDL/DML/select/show/set command smoke across Firebird, MySQL, and PostgreSQL parser surfaces
- Evidence suite run captured at:
  - `artifacts/cross_os/p6s2w2/xos-047-cross-os-smoke-ctest.txt`

## Validation
- Listener/runtime matrix:
  - `ListenerIpcAdapterTest.FrontDoorSocketLifecycle`
  - `ServiceControllerListenerBootstrapTest.DirectModeLaunchesNativePgMysqlFirebirdListenerMatrix`
- Parser DDL/DML smoke:
  - `FirebirdParserTest.CreateTableSimple`
  - `FirebirdParserTest.InsertSimple`
  - `FirebirdParserTest.SelectSimple`
  - `FirebirdParserTest.SetSqlDialect`
  - `MySQLParserTest.CreateTableBasic`
  - `MySQLParserTest.InsertBasic`
  - `MySQLParserTest.UpdateBasic`
  - `MySQLParserTest.SelectWithFrom`
  - `MySQLParserTest.UseStatement`
  - `PostgreSQLParserTest.CreateTableBasic`
  - `PostgreSQLParserTest.InsertBasic`
  - `PostgreSQLParserTest.UpdateBasic`
  - `PostgreSQLParserTest.SimpleSelect`
  - `PostgreSQLParserTest.SetAndShowStatements`
- Result:
  - `37/37` tests passed.

