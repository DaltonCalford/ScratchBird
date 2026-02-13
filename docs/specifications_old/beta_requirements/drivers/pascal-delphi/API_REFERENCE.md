# API Reference (Pascal)

## Wrapper Types
- JSONB: `TScratchBirdJsonb`
- RANGE: `TScratchBirdRange`
- GEOMETRY: `TScratchBirdGeometry`


## FireDAC
- TFDConnection: Open, Close, StartTransaction, Commit, Rollback, ExecSQL, Connected
- TFDQuery/TFDCommand: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, Eof,
  FieldByName, RowsAffected

## Zeos
- TZConnection: Connect, Disconnect, StartTransaction, Commit, Rollback
- TZQuery: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, EOF, FieldByName,
  RowsAffected

## SQLdb
- TSQLConnection: Open, Close, StartTransaction, Commit, Rollback
- TSQLQuery: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, EOF, FieldByName,
  RowsAffected

## IBX
- TIBDatabase: Open, Close, StartTransaction, Commit, Rollback
- TIBQuery: SQL.Text, Prepare, ParamByName, ExecSQL, Open, Next, EOF, FieldByName

## Usage examples (FireDAC)
```pascal
FDConnection.Params.Values['DriverID'] := 'Scratchbird';
FDConnection.Params.Values['Server'] := 'localhost';
FDConnection.Params.Values['Port'] := '3092';
FDConnection.Params.Values['Database'] := 'db';
FDConnection.Params.Values['User_Name'] := 'user';
FDConnection.Params.Values['Password'] := 'pass';
FDConnection.Open;

FDQuery.SQL.Text := 'select * from widgets where id = :id';
FDQuery.ParamByName('id').AsInteger := 42;
FDQuery.Open;
```
