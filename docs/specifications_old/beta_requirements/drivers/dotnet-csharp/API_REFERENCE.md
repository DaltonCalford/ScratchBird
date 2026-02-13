# API Reference (ADO.NET)

## Wrapper Types
- JSONB: `ScratchBirdJsonb`
- RANGE: `ScratchBirdRange<T>`
- GEOMETRY: `ScratchBirdGeometry`


## Provider factory
- DbProviderFactory.CreateConnection()
- DbProviderFactory.CreateCommand()
- DbProviderFactory.CreateParameter()
- DbProviderFactory.CreateDataAdapter() (optional)

## Connection
- DbConnection.ConnectionString
- DbConnection.Database
- DbConnection.State
- Open() / OpenAsync()
- Close()
- BeginTransaction(isolation)
- CreateCommand()
- ChangeDatabase(name) (optional)

## Command
- DbCommand.CommandText
- DbCommand.CommandType (Text required)
- DbCommand.CommandTimeout
- DbCommand.Parameters
- Prepare()
- ExecuteNonQuery() / ExecuteNonQueryAsync()
- ExecuteScalar() / ExecuteScalarAsync()
- ExecuteReader() / ExecuteReaderAsync()
- Cancel()

## Data reader
- Read() / ReadAsync()
- NextResult() / NextResultAsync()
- FieldCount
- GetName/GetFieldType/GetValue
- GetBoolean/GetInt32/GetInt64/GetDecimal/GetDateTime/GetString/GetGuid/GetBytes
- IsDBNull()
- RecordsAffected

## Parameters and transactions
- DbParameter.ParameterName
- DbParameter.DbType
- DbParameter.Value
- DbParameter.Direction
- DbParameter.Precision / Scale / Size
- DbTransaction.Commit() / Rollback()

## Usage examples
```csharp
using var conn = new ScratchbirdConnection(
    "Host=localhost;Port=3092;Database=db;Username=user;Password=pass");
conn.Open();

using var cmd = conn.CreateCommand();
cmd.CommandText = "select * from widgets where id = $1";
var p = cmd.CreateParameter();
p.ParameterName = "$1";
p.Value = 42;
cmd.Parameters.Add(p);

using var reader = cmd.ExecuteReader();
```
