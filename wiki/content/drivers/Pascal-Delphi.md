# Pascal / Delphi Driver Guide

**Status:** Complete
**Last Updated:** 2026-01-30

---

## Overview

ScratchBird supports multiple connection protocols for Delphi and Free Pascal applications:

| Protocol | Port | Component | Best For |
|----------|------|-----------|----------|
| Native | 3092 | ScratchBird Pascal/Delphi (SBWP v1.1) | Full ScratchBird feature set |
| PostgreSQL | 5432 | FireDAC (TFDConnection) | Ecosystem compatibility |
| MySQL | 3306 | FireDAC, Zeos | MySQL compatibility |
| Firebird | 3050 | FireDAC, IBX, FIBPlus | Firebird migration, legacy apps |

**Recommendation:** Use the **ScratchBird Pascal/Delphi native driver** for full SBWP v1.1 feature coverage. Use FireDAC/IBX/Zeos only when you need emulation compatibility.

---

## ScratchBird Native Driver (SBWP v1.1)

### Quick Start

```pascal
uses
  ScratchBird.Client;

var
  Client: TScratchBirdClient;
begin
  Client := TScratchBirdClient.Create;
  try
    Client.Connect('scratchbird://app_user:secret@localhost:3092/scratchbird');
    Client.ExecSQL('SELECT 1');
  finally
    Client.Free;
  end;
end;
```

Adapters are provided for FireDAC, IBX, Zeos, and SQLdb to reuse existing datasets while
still using the native ScratchBird protocol.
The native driver uses SBWP v1.1 with server-side prepare/bind and binary-only parameters.

## Part 1: Quick Start

### FireDAC Setup (RAD Studio)

1. Add FireDAC components to your form:
   - `TFDConnection`
   - `TFDQuery`
   - `TFDPhysPgDriverLink` (for PostgreSQL)

2. Configure connection:

```pascal
procedure TForm1.FormCreate(Sender: TObject);
begin
  FDConnection1.DriverName := 'PG';
  FDConnection1.Params.Values['Server'] := 'localhost';
  FDConnection1.Params.Values['Port'] := '5432';
  FDConnection1.Params.Values['Database'] := 'scratchbird';
  FDConnection1.Params.Values['User_Name'] := 'app_user';
  FDConnection1.Params.Values['Password'] := 'secret';

  try
    FDConnection1.Connected := True;
    ShowMessage('Connected to ScratchBird!');
  except
    on E: Exception do
      ShowMessage('Connection failed: ' + E.Message);
  end;
end;
```

### Install via sb_setup (Installer Utility)

If you installed ScratchBird with the installer, you can add the native driver pack later:

```bash
sb_setup --interactive
```

Select `scratchbird-driver-pascal` or the `scratchbird-drivers-all` meta package. On Linux, run with `sudo`.

### First Query

```pascal
procedure TForm1.btnQueryClick(Sender: TObject);
var
  Version: string;
begin
  FDQuery1.Connection := FDConnection1;
  FDQuery1.SQL.Text := 'SELECT version()';
  FDQuery1.Open;

  Version := FDQuery1.FieldByName('version').AsString;
  ShowMessage('Server version: ' + Version);

  FDQuery1.Close;
end;
```

---

## Part 2: FireDAC (PostgreSQL Protocol)

FireDAC is the recommended database access framework for RAD Studio when using **emulation**
protocols (PostgreSQL/MySQL/Firebird).

### Connection Configuration

**Design-time setup:**
1. Drop `TFDConnection` on form
2. Set `DriverName` to `PG`
3. Double-click to open Connection Editor
4. Fill in server details

**Runtime configuration:**
```pascal
procedure ConfigureConnection(AConnection: TFDConnection);
begin
  AConnection.DriverName := 'PG';

  with AConnection.Params do
  begin
    Clear;
    Add('Server=localhost');
    Add('Port=5432');
    Add('Database=scratchbird');
    Add('User_Name=app_user');
    Add('Password=secret');

    // Optional settings
    Add('CharacterSet=UTF8');
    Add('ApplicationName=MyApp');
    Add('LoginTimeout=10');
    Add('CommandTimeout=30');
  end;
end;
```

**Connection string format:**
```pascal
FDConnection1.ConnectionDefName := '';
FDConnection1.Params.Text :=
  'DriverID=PG' + sLineBreak +
  'Server=localhost' + sLineBreak +
  'Port=5432' + sLineBreak +
  'Database=scratchbird' + sLineBreak +
  'User_Name=app_user' + sLineBreak +
  'Password=secret';
```

**SSL/TLS connection:**
```pascal
with FDConnection1.Params do
begin
  Add('Server=localhost');
  Add('Port=5432');
  Add('Database=scratchbird');
  Add('User_Name=app_user');
  Add('Password=secret');
  Add('PGAdvanced=sslmode=require');
end;
```

### CRUD Operations

**Create (INSERT):**
```pascal
procedure InsertUser(AConnection: TFDConnection; const AName, AEmail: string);
var
  Query: TFDQuery;
  NewID: Integer;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;

    // Using parameters (recommended)
    Query.SQL.Text :=
      'INSERT INTO users (name, email, created_at) ' +
      'VALUES (:name, :email, CURRENT_TIMESTAMP) ' +
      'RETURNING id';

    Query.ParamByName('name').AsString := AName;
    Query.ParamByName('email').AsString := AEmail;

    Query.Open;
    NewID := Query.FieldByName('id').AsInteger;
    ShowMessage('Created user with ID: ' + IntToStr(NewID));
  finally
    Query.Free;
  end;
end;
```

**Read (SELECT):**
```pascal
procedure LoadUsers(AConnection: TFDConnection; ADataSet: TFDQuery);
begin
  ADataSet.Connection := AConnection;
  ADataSet.SQL.Text := 'SELECT id, name, email, created_at FROM users WHERE active = :active';
  ADataSet.ParamByName('active').AsBoolean := True;
  ADataSet.Open;
end;

procedure DisplayUser(AConnection: TFDConnection; AUserID: Integer);
var
  Query: TFDQuery;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;
    Query.SQL.Text := 'SELECT * FROM users WHERE id = :id';
    Query.ParamByName('id').AsInteger := AUserID;
    Query.Open;

    if not Query.IsEmpty then
    begin
      ShowMessage(Format('User: %s <%s>',
        [Query.FieldByName('name').AsString,
         Query.FieldByName('email').AsString]));
    end
    else
      ShowMessage('User not found');
  finally
    Query.Free;
  end;
end;
```

**Update:**
```pascal
procedure UpdateUserEmail(AConnection: TFDConnection; AUserID: Integer;
  const ANewEmail: string);
var
  Query: TFDQuery;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;
    Query.SQL.Text :=
      'UPDATE users SET email = :email, updated_at = CURRENT_TIMESTAMP ' +
      'WHERE id = :id';
    Query.ParamByName('email').AsString := ANewEmail;
    Query.ParamByName('id').AsInteger := AUserID;
    Query.ExecSQL;

    ShowMessage(Format('Updated %d rows', [Query.RowsAffected]));
  finally
    Query.Free;
  end;
end;
```

**Delete:**
```pascal
procedure DeleteUser(AConnection: TFDConnection; AUserID: Integer);
var
  Query: TFDQuery;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;
    Query.SQL.Text := 'DELETE FROM users WHERE id = :id';
    Query.ParamByName('id').AsInteger := AUserID;
    Query.ExecSQL;

    ShowMessage(Format('Deleted %d rows', [Query.RowsAffected]));
  finally
    Query.Free;
  end;
end;
```

### Parameters

**Parameter types:**
```pascal
Query.SQL.Text :=
  'INSERT INTO products (name, price, quantity, active, metadata) ' +
  'VALUES (:name, :price, :quantity, :active, :metadata)';

Query.ParamByName('name').AsString := 'Laptop';
Query.ParamByName('price').AsCurrency := 999.99;
Query.ParamByName('quantity').AsInteger := 10;
Query.ParamByName('active').AsBoolean := True;
Query.ParamByName('metadata').AsString := '{"category": "electronics"}';

Query.ExecSQL;
```

**Array parameters (PostgreSQL):**
```pascal
Query.SQL.Text := 'SELECT * FROM users WHERE id = ANY(:ids)';
Query.ParamByName('ids').DataType := ftArray;
Query.ParamByName('ids').AsArray := VarArrayOf([1, 2, 3, 4, 5]);
Query.Open;
```

**NULL values:**
```pascal
if AValue = '' then
  Query.ParamByName('description').Clear  // Sets to NULL
else
  Query.ParamByName('description').AsString := AValue;
```

### Transactions

**Basic transaction:**
```pascal
procedure TransferFunds(AConnection: TFDConnection;
  AFromAccount, AToAccount: Integer; AAmount: Currency);
begin
  AConnection.StartTransaction;
  try
    // Debit source account
    AConnection.ExecSQL(
      'UPDATE accounts SET balance = balance - :amount WHERE id = :id',
      [AAmount, AFromAccount]);

    // Credit destination account
    AConnection.ExecSQL(
      'UPDATE accounts SET balance = balance + :amount WHERE id = :id',
      [AAmount, AToAccount]);

    AConnection.Commit;
    ShowMessage('Transfer completed');
  except
    on E: Exception do
    begin
      AConnection.Rollback;
      ShowMessage('Transfer failed: ' + E.Message);
      raise;
    end;
  end;
end;
```

**Transaction options:**
```pascal
var
  TxOptions: TFDTxOptions;
begin
  TxOptions := TFDTxOptions.Create;
  try
    TxOptions.Isolation := xiSerializable;
    TxOptions.ReadOnly := False;

    FDConnection1.StartTransaction(TxOptions);
    try
      // ... operations
      FDConnection1.Commit;
    except
      FDConnection1.Rollback;
      raise;
    end;
  finally
    TxOptions.Free;
  end;
end;
```

**Savepoints:**
```pascal
procedure CreateOrderWithItems(AConnection: TFDConnection);
var
  OrderID: Integer;
begin
  AConnection.StartTransaction;
  try
    // Create order
    AConnection.ExecSQL(
      'INSERT INTO orders (customer_id) VALUES (:customer_id) RETURNING id',
      [1], [OrderID]);

    // Savepoint before items
    AConnection.ExecSQL('SAVEPOINT before_items', []);

    try
      // Add items
      AConnection.ExecSQL(
        'INSERT INTO order_items (order_id, product_id, quantity) VALUES (:oid, :pid, :qty)',
        [OrderID, 999, 1]);
    except
      // Rollback to savepoint, keep the order
      AConnection.ExecSQL('ROLLBACK TO SAVEPOINT before_items', []);
    end;

    AConnection.Commit;
  except
    AConnection.Rollback;
    raise;
  end;
end;
```

### Batch Operations

**Array DML:**
```pascal
procedure BatchInsertUsers(AConnection: TFDConnection; AUsers: TArray<TUser>);
var
  Query: TFDQuery;
  I: Integer;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;
    Query.SQL.Text := 'INSERT INTO users (name, email) VALUES (:name, :email)';

    // Enable array DML
    Query.Params.ArraySize := Length(AUsers);

    for I := 0 to High(AUsers) do
    begin
      Query.ParamByName('name').AsStrings[I] := AUsers[I].Name;
      Query.ParamByName('email').AsStrings[I] := AUsers[I].Email;
    end;

    Query.Execute(Length(AUsers), 0);
    ShowMessage(Format('Inserted %d users', [Query.RowsAffected]));
  finally
    Query.Free;
  end;
end;
```

**Batch execute in transaction:**
```pascal
procedure BatchUpdate(AConnection: TFDConnection);
var
  Query: TFDQuery;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;
    Query.SQL.Text := 'UPDATE products SET price = price * 1.1 WHERE category = :cat';

    AConnection.StartTransaction;
    try
      Query.ParamByName('cat').AsString := 'electronics';
      Query.ExecSQL;

      Query.ParamByName('cat').AsString := 'clothing';
      Query.ExecSQL;

      AConnection.Commit;
    except
      AConnection.Rollback;
      raise;
    end;
  finally
    Query.Free;
  end;
end;
```

### Master-Detail Relationships

```pascal
// Design-time or runtime setup
procedure SetupMasterDetail(AMaster, ADetail: TFDQuery);
begin
  // Master query
  AMaster.SQL.Text := 'SELECT * FROM customers ORDER BY name';

  // Detail query with parameter linked to master
  ADetail.SQL.Text := 'SELECT * FROM orders WHERE customer_id = :customer_id';
  ADetail.MasterSource := DataSourceMaster;  // TDataSource linked to AMaster
  ADetail.MasterFields := 'id';
  ADetail.IndexFieldNames := 'customer_id';

  AMaster.Open;
  // ADetail opens automatically when navigating master
end;
```

### Cached Updates

```pascal
procedure UseCachedUpdates(AQuery: TFDQuery);
begin
  AQuery.CachedUpdates := True;
  AQuery.Open;

  // Make changes
  AQuery.Edit;
  AQuery.FieldByName('name').AsString := 'New Name';
  AQuery.Post;

  AQuery.Append;
  AQuery.FieldByName('name').AsString := 'Another User';
  AQuery.FieldByName('email').AsString := 'another@example.com';
  AQuery.Post;

  // Apply all changes to database
  if AQuery.ApplyUpdates(0) > 0 then
    AQuery.CommitUpdates
  else
    ShowMessage('Updates applied successfully');
end;
```

### Connection Pooling

```pascal
// Configure pooling in connection definition
FDManager1.ConnectionDefName := 'ScratchBird_Pooled';
FDManager1.ConnectionDefs.AddConnectionDef;

with FDManager1.ConnectionDefs[0] do
begin
  Name := 'ScratchBird_Pooled';
  DriverID := 'PG';
  Params.Add('Server=localhost');
  Params.Add('Port=5432');
  Params.Add('Database=scratchbird');
  Params.Add('User_Name=app_user');
  Params.Add('Password=secret');
  Params.Add('Pooled=True');
  Params.Add('POOL_MaximumItems=50');
  Params.Add('POOL_ExpireTimeout=90000');
  Params.Add('POOL_CleanupTimeout=30000');
end;

// Use pooled connection
FDConnection1.ConnectionDefName := 'ScratchBird_Pooled';
FDConnection1.Connected := True;
```

---

## Part 3: FireDAC (Firebird Protocol)

For applications migrating from Firebird or requiring Firebird SQL dialect.

### Connection Setup

```pascal
procedure ConfigureFirebirdConnection(AConnection: TFDConnection);
begin
  AConnection.DriverName := 'FB';

  with AConnection.Params do
  begin
    Clear;
    Add('Server=localhost');
    Add('Port=3050');
    Add('Database=scratchbird');
    Add('User_Name=SYSDBA');
    Add('Password=masterkey');
    Add('CharacterSet=UTF8');
  end;
end;
```

### Firebird-Specific Queries

```pascal
// FIRST/SKIP pagination
Query.SQL.Text :=
  'SELECT FIRST 10 SKIP 0 id, name, email ' +
  'FROM users ' +
  'ORDER BY name';
Query.Open;

// UPDATE OR INSERT (upsert)
Query.SQL.Text :=
  'UPDATE OR INSERT INTO users (id, name, email) ' +
  'VALUES (:id, :name, :email) ' +
  'MATCHING (id)';
Query.ParamByName('id').AsInteger := 1;
Query.ParamByName('name').AsString := 'Updated Name';
Query.ParamByName('email').AsString := 'updated@example.com';
Query.ExecSQL;

// EXECUTE BLOCK
Query.SQL.Text :=
  'EXECUTE BLOCK (p_customer_id INTEGER = :customer_id) ' +
  'RETURNS (order_count INTEGER, total_amount DECIMAL(18,2)) ' +
  'AS ' +
  'BEGIN ' +
  '  SELECT COUNT(*), COALESCE(SUM(amount), 0) ' +
  '  FROM orders ' +
  '  WHERE customer_id = :p_customer_id ' +
  '  INTO :order_count, :total_amount; ' +
  '  SUSPEND; ' +
  'END';
Query.ParamByName('customer_id').AsInteger := 1;
Query.Open;
```

### Generators/Sequences

```pascal
function GetNextID(AConnection: TFDConnection; const AGeneratorName: string): Int64;
var
  Query: TFDQuery;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;
    Query.SQL.Text := 'SELECT GEN_ID(' + AGeneratorName + ', 1) FROM RDB$DATABASE';
    Query.Open;
    Result := Query.Fields[0].AsLargeInt;
  finally
    Query.Free;
  end;
end;
```

---

## Part 4: FireDAC (MySQL Protocol)

### Connection Setup

```pascal
procedure ConfigureMySQLConnection(AConnection: TFDConnection);
begin
  AConnection.DriverName := 'MySQL';

  with AConnection.Params do
  begin
    Clear;
    Add('Server=localhost');
    Add('Port=3306');
    Add('Database=scratchbird');
    Add('User_Name=app_user');
    Add('Password=secret');
    Add('CharacterSet=utf8mb4');
  end;
end;
```

### MySQL-Specific Features

```pascal
// Last insert ID
Query.SQL.Text := 'INSERT INTO users (name, email) VALUES (:name, :email)';
Query.ParamByName('name').AsString := 'Alice';
Query.ParamByName('email').AsString := 'alice@example.com';
Query.ExecSQL;

// Get last insert ID
Query.SQL.Text := 'SELECT LAST_INSERT_ID()';
Query.Open;
NewID := Query.Fields[0].AsInteger;
```

---

## Part 5: IBX (InterBase Express)

Legacy component set for Firebird/InterBase databases.

### Connection Setup

```pascal
uses IBDatabase, IBQuery;

procedure ConfigureIBConnection(ADatabase: TIBDatabase);
begin
  ADatabase.DatabaseName := 'localhost/3050:scratchbird';
  ADatabase.Params.Clear;
  ADatabase.Params.Add('user_name=SYSDBA');
  ADatabase.Params.Add('password=masterkey');
  ADatabase.Params.Add('lc_ctype=UTF8');

  ADatabase.LoginPrompt := False;
  ADatabase.Connected := True;
end;
```

### IBX CRUD Operations

```pascal
procedure IBXExample(ADatabase: TIBDatabase; ATransaction: TIBTransaction);
var
  Query: TIBQuery;
begin
  Query := TIBQuery.Create(nil);
  try
    Query.Database := ADatabase;
    Query.Transaction := ATransaction;

    ATransaction.StartTransaction;
    try
      // Insert
      Query.SQL.Text :=
        'INSERT INTO users (name, email) VALUES (:name, :email) RETURNING id';
      Query.ParamByName('name').AsString := 'Bob';
      Query.ParamByName('email').AsString := 'bob@example.com';
      Query.Open;
      ShowMessage('New ID: ' + Query.FieldByName('id').AsString);
      Query.Close;

      // Select
      Query.SQL.Text := 'SELECT * FROM users WHERE active = :active';
      Query.ParamByName('active').AsBoolean := True;
      Query.Open;

      while not Query.Eof do
      begin
        // Process row
        Query.Next;
      end;

      ATransaction.Commit;
    except
      ATransaction.Rollback;
      raise;
    end;
  finally
    Query.Free;
  end;
end;
```

---

## Part 6: Zeos Database Objects

Cross-platform open-source database components for Delphi and Free Pascal.

### Installation

1. Download from SourceForge or GitHub
2. Add packages to IDE
3. Add `ZConnection`, `ZQuery` to uses clause

### Connection Setup

```pascal
uses ZConnection, ZDataset;

procedure ConfigureZeosConnection(AConnection: TZConnection);
begin
  // PostgreSQL
  AConnection.Protocol := 'postgresql';
  AConnection.HostName := 'localhost';
  AConnection.Port := 5432;
  AConnection.Database := 'scratchbird';
  AConnection.User := 'app_user';
  AConnection.Password := 'secret';

  // Or MySQL
  // AConnection.Protocol := 'mysql';
  // AConnection.Port := 3306;

  // Or Firebird
  // AConnection.Protocol := 'firebird';
  // AConnection.Port := 3050;

  AConnection.Connect;
end;
```

### Zeos CRUD Operations

```pascal
procedure ZeosExample(AConnection: TZConnection);
var
  Query: TZQuery;
begin
  Query := TZQuery.Create(nil);
  try
    Query.Connection := AConnection;

    // Insert
    Query.SQL.Text :=
      'INSERT INTO users (name, email) VALUES (:name, :email) RETURNING id';
    Query.ParamByName('name').AsString := 'Charlie';
    Query.ParamByName('email').AsString := 'charlie@example.com';
    Query.Open;
    ShowMessage('Created ID: ' + Query.FieldByName('id').AsString);
    Query.Close;

    // Select
    Query.SQL.Text := 'SELECT * FROM users ORDER BY name';
    Query.Open;

    while not Query.Eof do
    begin
      ShowMessage(Query.FieldByName('name').AsString);
      Query.Next;
    end;

    // Update
    Query.SQL.Text := 'UPDATE users SET email = :email WHERE id = :id';
    Query.ParamByName('email').AsString := 'charlie.new@example.com';
    Query.ParamByName('id').AsInteger := 1;
    Query.ExecSQL;

  finally
    Query.Free;
  end;
end;
```

### Zeos Transactions

```pascal
procedure ZeosTransaction(AConnection: TZConnection);
var
  Query: TZQuery;
begin
  Query := TZQuery.Create(nil);
  try
    Query.Connection := AConnection;

    AConnection.StartTransaction;
    try
      Query.SQL.Text := 'UPDATE accounts SET balance = balance - :amount WHERE id = :id';
      Query.ParamByName('amount').AsCurrency := 100;
      Query.ParamByName('id').AsInteger := 1;
      Query.ExecSQL;

      Query.SQL.Text := 'UPDATE accounts SET balance = balance + :amount WHERE id = :id';
      Query.ParamByName('amount').AsCurrency := 100;
      Query.ParamByName('id').AsInteger := 2;
      Query.ExecSQL;

      AConnection.Commit;
    except
      AConnection.Rollback;
      raise;
    end;
  finally
    Query.Free;
  end;
end;
```

---

## Part 7: Free Pascal / Lazarus

### SQLdb Components

```pascal
uses sqldb, pqconnection;

procedure FreePascalExample;
var
  Conn: TPQConnection;
  Trans: TSQLTransaction;
  Query: TSQLQuery;
begin
  Conn := TPQConnection.Create(nil);
  Trans := TSQLTransaction.Create(nil);
  Query := TSQLQuery.Create(nil);
  try
    // Configure connection
    Conn.HostName := 'localhost';
    Conn.DatabaseName := 'scratchbird';
    Conn.UserName := 'app_user';
    Conn.Password := 'secret';
    Conn.Port := 5432;
    Conn.Transaction := Trans;

    // Configure transaction
    Trans.Database := Conn;

    // Configure query
    Query.Database := Conn;
    Query.Transaction := Trans;

    // Connect
    Conn.Connected := True;
    Trans.Active := True;

    // Query
    Query.SQL.Text := 'SELECT * FROM users WHERE active = :active';
    Query.ParamByName('active').AsBoolean := True;
    Query.Open;

    while not Query.EOF do
    begin
      WriteLn(Query.FieldByName('name').AsString);
      Query.Next;
    end;

    Trans.Commit;
  finally
    Query.Free;
    Trans.Free;
    Conn.Free;
  end;
end;
```

### MySQL Connection (Free Pascal)

```pascal
uses sqldb, mysql80conn;

procedure MySQLExample;
var
  Conn: TMySQL80Connection;
  Trans: TSQLTransaction;
  Query: TSQLQuery;
begin
  Conn := TMySQL80Connection.Create(nil);
  try
    Conn.HostName := 'localhost';
    Conn.DatabaseName := 'scratchbird';
    Conn.UserName := 'app_user';
    Conn.Password := 'secret';
    Conn.Port := 3306;

    // ... rest of setup
  finally
    Conn.Free;
  end;
end;
```

---

## Part 8: Data-Aware Controls

### Binding to Grid

```pascal
procedure BindToGrid(AConnection: TFDConnection; AGrid: TDBGrid);
var
  Query: TFDQuery;
  DataSource: TDataSource;
begin
  Query := TFDQuery.Create(AGrid.Owner);
  DataSource := TDataSource.Create(AGrid.Owner);

  Query.Connection := AConnection;
  Query.SQL.Text := 'SELECT id, name, email, created_at FROM users ORDER BY name';

  DataSource.DataSet := Query;
  AGrid.DataSource := DataSource;

  Query.Open;
end;
```

### Editable Grid

```pascal
procedure SetupEditableGrid(AQuery: TFDQuery);
begin
  AQuery.SQL.Text := 'SELECT * FROM users ORDER BY name';
  AQuery.UpdateOptions.UpdateTableName := 'users';
  AQuery.UpdateOptions.KeyFields := 'id';
  AQuery.UpdateOptions.AutoIncFields := 'id';
  AQuery.Open;

  // Changes in grid are automatically tracked
  // Call AQuery.ApplyUpdates to save or Post for immediate save
end;
```

### Master-Detail with Grids

```pascal
procedure SetupMasterDetailGrids(AConnection: TFDConnection;
  AMasterGrid, ADetailGrid: TDBGrid);
var
  MasterQuery, DetailQuery: TFDQuery;
  MasterDS, DetailDS: TDataSource;
begin
  MasterQuery := TFDQuery.Create(nil);
  DetailQuery := TFDQuery.Create(nil);
  MasterDS := TDataSource.Create(nil);
  DetailDS := TDataSource.Create(nil);

  // Master setup
  MasterQuery.Connection := AConnection;
  MasterQuery.SQL.Text := 'SELECT * FROM customers ORDER BY name';
  MasterDS.DataSet := MasterQuery;
  AMasterGrid.DataSource := MasterDS;

  // Detail setup
  DetailQuery.Connection := AConnection;
  DetailQuery.SQL.Text := 'SELECT * FROM orders WHERE customer_id = :customer_id';
  DetailQuery.MasterSource := MasterDS;
  DetailQuery.MasterFields := 'id';
  DetailQuery.IndexFieldNames := 'customer_id';
  DetailDS.DataSet := DetailQuery;
  ADetailGrid.DataSource := DetailDS;

  MasterQuery.Open;
  // DetailQuery opens automatically
end;
```

---

## Part 9: Error Handling

### FireDAC Exceptions

```pascal
uses FireDAC.Stan.Error;

procedure HandleFireDACError(AConnection: TFDConnection);
begin
  try
    AConnection.ExecSQL('INSERT INTO users (email) VALUES (:email)',
      ['duplicate@example.com']);
  except
    on E: EFDDBEngineException do
    begin
      case E.Kind of
        ekUKViolated:
          ShowMessage('Duplicate value: ' + E.Message);
        ekFKViolated:
          ShowMessage('Foreign key violation: ' + E.Message);
        ekRecordLocked:
          ShowMessage('Record is locked by another user');
        ekServerGone:
          ShowMessage('Connection to server lost');
        else
          ShowMessage(Format('Database error [%d]: %s', [E.ErrorCode, E.Message]));
      end;
    end;
    on E: Exception do
      ShowMessage('Error: ' + E.Message);
  end;
end;
```

### PostgreSQL Error Codes

```pascal
procedure HandlePostgreSQLError(E: EFDDBEngineException);
var
  SQLState: string;
begin
  // Get SQLSTATE from error
  if E.ErrorCount > 0 then
    SQLState := E.Errors[0].SQLState;

  case SQLState of
    '23505': ShowMessage('Unique constraint violation');
    '23503': ShowMessage('Foreign key violation');
    '23502': ShowMessage('NOT NULL violation');
    '42P01': ShowMessage('Table does not exist');
    '42703': ShowMessage('Column does not exist');
    '08006': ShowMessage('Connection failure');
    '40001': ShowMessage('Serialization failure - retry transaction');
    '40P01': ShowMessage('Deadlock detected');
  else
    ShowMessage(Format('PostgreSQL error [%s]: %s', [SQLState, E.Message]));
  end;
end;
```

### Retry Logic

```pascal
function ExecuteWithRetry(AConnection: TFDConnection; const ASQL: string;
  AParams: array of const; AMaxRetries: Integer = 3): Integer;
var
  Attempt: Integer;
  Delay: Integer;
begin
  Result := 0;
  Attempt := 0;
  Delay := 100;

  while Attempt < AMaxRetries do
  begin
    try
      Result := AConnection.ExecSQL(ASQL, AParams);
      Exit;
    except
      on E: EFDDBEngineException do
      begin
        Inc(Attempt);

        // Only retry transient errors
        if not (E.Kind in [ekServerGone, ekRecordLocked]) then
          raise;

        if Attempt >= AMaxRetries then
          raise;

        Sleep(Delay);
        Delay := Delay * 2;  // Exponential backoff

        // Reconnect if needed
        if not AConnection.Connected then
          AConnection.Connected := True;
      end;
    end;
  end;
end;
```

---

## Part 10: Special Data Types

### JSON/JSONB

```pascal
uses System.JSON;

procedure WorkWithJSON(AConnection: TFDConnection);
var
  Query: TFDQuery;
  Settings: TJSONObject;
  JSONStr: string;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;

    // Insert JSON
    Settings := TJSONObject.Create;
    try
      Settings.AddPair('theme', 'dark');
      Settings.AddPair('language', 'en');
      Settings.AddPair('notify', TJSONBool.Create(True));
      JSONStr := Settings.ToString;
    finally
      Settings.Free;
    end;

    Query.SQL.Text :=
      'INSERT INTO user_settings (user_id, settings) VALUES (:user_id, :settings::jsonb)';
    Query.ParamByName('user_id').AsInteger := 1;
    Query.ParamByName('settings').AsString := JSONStr;
    Query.ExecSQL;

    // Query JSON
    Query.SQL.Text :=
      'SELECT settings->>''theme'' as theme FROM user_settings WHERE user_id = :id';
    Query.ParamByName('id').AsInteger := 1;
    Query.Open;

    ShowMessage('Theme: ' + Query.FieldByName('theme').AsString);
  finally
    Query.Free;
  end;
end;
```

### BLOBs

```pascal
procedure WorkWithBLOB(AConnection: TFDConnection);
var
  Query: TFDQuery;
  Stream: TFileStream;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;

    // Insert BLOB
    Query.SQL.Text := 'INSERT INTO documents (name, content) VALUES (:name, :content)';
    Query.ParamByName('name').AsString := 'report.pdf';

    Stream := TFileStream.Create('C:\Reports\report.pdf', fmOpenRead);
    try
      Query.ParamByName('content').LoadFromStream(Stream, ftBlob);
    finally
      Stream.Free;
    end;
    Query.ExecSQL;

    // Read BLOB
    Query.SQL.Text := 'SELECT content FROM documents WHERE name = :name';
    Query.ParamByName('name').AsString := 'report.pdf';
    Query.Open;

    Stream := TFileStream.Create('C:\Temp\downloaded.pdf', fmCreate);
    try
      TBlobField(Query.FieldByName('content')).SaveToStream(Stream);
    finally
      Stream.Free;
    end;
  finally
    Query.Free;
  end;
end;
```

### Date/Time

```pascal
procedure WorkWithDateTime(AConnection: TFDConnection);
var
  Query: TFDQuery;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := AConnection;

    // Insert with Delphi TDateTime
    Query.SQL.Text :=
      'INSERT INTO events (name, event_date, start_time, created_at) ' +
      'VALUES (:name, :event_date, :start_time, :created_at)';

    Query.ParamByName('name').AsString := 'Conference';
    Query.ParamByName('event_date').AsDate := EncodeDate(2026, 6, 15);
    Query.ParamByName('start_time').AsTime := EncodeTime(9, 0, 0, 0);
    Query.ParamByName('created_at').AsDateTime := Now;
    Query.ExecSQL;

    // Read DateTime
    Query.SQL.Text := 'SELECT * FROM events WHERE id = :id';
    Query.ParamByName('id').AsInteger := 1;
    Query.Open;

    ShowMessage(Format('Event on %s at %s',
      [DateToStr(Query.FieldByName('event_date').AsDateTime),
       TimeToStr(Query.FieldByName('start_time').AsDateTime)]));
  finally
    Query.Free;
  end;
end;
```

---

## Part 11: Common Issues

### Connection Issues

**"Could not connect to server":**
```pascal
// Check server is running and accessible
// Verify firewall allows port 5432/3306/3050

// Increase timeout
FDConnection1.Params.Add('LoginTimeout=30');

// Test with ping first
if FDConnection1.Ping then
  FDConnection1.Connected := True;
```

### Character Encoding

```pascal
// PostgreSQL
FDConnection1.Params.Add('CharacterSet=UTF8');

// Firebird
FDConnection1.Params.Add('CharacterSet=UTF8');

// MySQL
FDConnection1.Params.Add('CharacterSet=utf8mb4');
```

### Memory Leaks

```pascal
// Always free dynamically created objects
Query := TFDQuery.Create(nil);
try
  // Use query
finally
  Query.Free;  // IMPORTANT!
end;

// Or use owner for automatic cleanup
Query := TFDQuery.Create(Form1);  // Freed when Form1 is freed
```

### Concurrent Access

```pascal
// Use separate connections for threads
procedure TMyThread.Execute;
var
  LocalConnection: TFDConnection;
  Query: TFDQuery;
begin
  LocalConnection := TFDConnection.Create(nil);
  Query := TFDQuery.Create(nil);
  try
    // Configure LocalConnection (copy settings from main connection)
    LocalConnection.DriverName := 'PG';
    // ... other settings

    LocalConnection.Connected := True;
    Query.Connection := LocalConnection;

    // Thread-safe operations
  finally
    Query.Free;
    LocalConnection.Free;
  end;
end;
```

---

## Quick Reference

### Connection Strings

**FireDAC PostgreSQL:**
```
DriverID=PG;Server=localhost;Port=5432;Database=scratchbird;User_Name=app_user;Password=secret
```

**FireDAC MySQL:**
```
DriverID=MySQL;Server=localhost;Port=3306;Database=scratchbird;User_Name=app_user;Password=secret
```

**FireDAC Firebird:**
```
DriverID=FB;Server=localhost;Port=3050;Database=scratchbird;User_Name=SYSDBA;Password=masterkey
```

### Component Summary

| Component | Framework | Protocol | Best For |
|-----------|-----------|----------|----------|
| TFDConnection/TFDQuery | FireDAC | All | RAD Studio (emulation) |
| TIBDatabase/TIBQuery | IBX | Firebird | Legacy apps |
| TZConnection/TZQuery | Zeos | All | Cross-platform |
| TPQConnection/TSQLQuery | SQLdb | PostgreSQL | Free Pascal |

### Common Operations

| Operation | FireDAC | IBX | Zeos |
|-----------|---------|-----|------|
| Connect | `Connected := True` | `Connected := True` | `Connect` |
| Execute | `ExecSQL` | `ExecSQL` | `ExecSQL` |
| Open query | `Open` | `Open` | `Open` |
| Start tx | `StartTransaction` | `StartTransaction` | `StartTransaction` |
| Commit | `Commit` | `Commit` | `Commit` |
| Rollback | `Rollback` | `Rollback` | `Rollback` |
| Param by name | `ParamByName('x')` | `ParamByName('x')` | `ParamByName('x')` |
| Field value | `FieldByName('x').AsString` | `FieldByName('x').AsString` | `FieldByName('x').AsString` |

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all available drivers
- [Desktop App Tutorial](../tutorials/Desktop-App-Delphi.md) - Build a Delphi application
- [Firebird Migration](../migration/From-Firebird.md) - Migrate from Firebird
- [Connection Guide](../getting-started/first-connection.md) - First connection walkthrough
