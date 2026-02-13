# API Reference (Go database/sql)

## Wrapper Types
- JSONB: `scratchbird.JSONB`
- RANGE: `scratchbird.Range[T]`
- GEOMETRY: `scratchbird.Geometry`


## Driver and connector
- driver.Driver.Open(name string) (driver.Conn, error)
- driver.DriverContext.OpenConnector(name string) (driver.Connector, error)
- driver.Connector.Connect(ctx) (driver.Conn, error)
- driver.Connector.Driver() driver.Driver

## Connection
- PrepareContext(ctx, query) (driver.Stmt, error)
- BeginTx(ctx, opts) (driver.Tx, error)
- ExecContext(ctx, query, args) (driver.Result, error)
- QueryContext(ctx, query, args) (driver.Rows, error)
- Ping(ctx) error
- ResetSession(ctx) error (optional)
- Close() error
- NamedValueChecker.CheckNamedValue(*driver.NamedValue) error

## Statement
- ExecContext(ctx, args) (driver.Result, error)
- QueryContext(ctx, args) (driver.Rows, error)
- NumInput() int
- Close() error

## Transaction
- Commit() error
- Rollback() error

## Rows and metadata
- Columns() []string
- Next(dest []driver.Value) error
- Close() error
- Optional column methods: ColumnTypeDatabaseTypeName, ColumnTypeNullable,
  ColumnTypeLength, ColumnTypePrecisionScale, ColumnTypeScanType

## Usage examples
```go
db, err := sql.Open("scratchbird", "scratchbird://user:pass@localhost:3092/db")
if err != nil {
  return err
}
defer db.Close()

row := db.QueryRow("select 1 as one")
_ = row

stmt, err := db.Prepare("select * from widgets where id = $1")
if err != nil {
  return err
}
rows, err := stmt.Query(42)
_ = rows
```
