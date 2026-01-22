# Pascal/Delphi/FreePascal Driver Specification
**Priority:** P0 (Critical - Beta Required - Firebird Migration Strategy)
**Status:** Draft
**Target Market:** Firebird developer base, Delphi/Lazarus developers, legacy enterprise applications
**Use Cases:** Firebird migration, Delphi desktop applications, Lazarus cross-platform apps, enterprise LOB apps

---


## ScratchBird V2 Scope (Native Protocol Only)

- Protocol: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- Default port: 3092.
- TLS 1.3 required.
- Emulated protocol drivers (PostgreSQL/MySQL/Firebird/TDS) are out of scope.


## Wrapper Types
- JSONB: `TScratchBirdJsonb`
- RANGE: `TScratchBirdRange`
- GEOMETRY: `TScratchBirdGeometry`

## Overview

Pascal/Delphi has a large and established developer base, particularly in the Firebird ecosystem. Since ScratchBird implements Firebird's MGA transaction model, providing excellent Delphi/Lazarus/FreePascal support is critical for attracting Firebird users and enabling seamless migration.

**Strategic Importance:**
- Firebird has a massive Pascal/Delphi user base
- Many Firebird applications are built with Delphi, Lazarus, or FreePascal
- Enables migration path from Firebird to ScratchBird
- Leverages existing FireDAC/IBX/Zeos component familiarity
- Targets enterprise line-of-business applications

**Key Requirements:**
- FireDAC driver (Delphi's database access framework)
- IBX (InterBase Express) compatibility layer
- Zeos database components support
- FreePascal/Lazarus SQLdb components
- Support for Delphi 7, Delphi XE-11, Delphi 12
- FreePascal 3.x compatibility
- Transaction management (Firebird-style)
- Blob streaming support
- Array field support
- Native Delphi types (TDateTime, Currency, etc.)

## Usage examples

```pascal
FDConnection.Params.Values['DriverID'] := 'Scratchbird';
FDConnection.Params.Values['Server'] := 'localhost';
FDConnection.Params.Values['Port'] := '3092';
FDConnection.Params.Values['Database'] := 'db';
FDConnection.Params.Values['User_Name'] := 'user';
FDConnection.Params.Values['Password'] := 'pass';
FDConnection.Open;

FDQuery.SQL.Text := 'select 1 as one';
FDQuery.Open;

FDQuery.SQL.Text := 'select * from widgets where id = :id';
FDQuery.ParamByName('id').AsInteger := 42;
FDQuery.Open;
```

---

## Specification Documents

### Required Documents

- [x] [SPECIFICATION.md](SPECIFICATION.md) - Detailed technical specification
  - FireDAC driver implementation
  - IBX compatibility layer
  - Zeos driver adapter
  - SQLdb components (FreePascal/Lazarus)
  - Type mappings (Pascal ↔ ScratchBird)
  - Transaction handling (Firebird-compatible)
  - Connection pooling
  - Event notifications (Firebird events equivalent)
  - Blob and array handling

- [x] [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) - Development roadmap
  - Milestone 1: FireDAC driver (Delphi 10.4+)
  - Milestone 2: IBX compatibility layer
  - Milestone 3: Zeos components support
  - Milestone 4: FreePascal/Lazarus SQLdb
  - Milestone 5: Firebird migration tools
  - Milestone 6: Legacy Delphi support (BDE replacement)
  - Resource requirements
  - Timeline estimates

- [x] [TESTING_CRITERIA.md](TESTING_CRITERIA.md) - Test requirements
  - FireDAC compliance tests
  - IBX compatibility tests
  - Zeos component tests
  - SQLdb (Lazarus) tests
  - Firebird application migration tests
  - Performance benchmarks vs Firebird dbExpress
  - Transaction isolation tests (Firebird MGA compatibility)
  - Multi-generational architecture verification

- [x] [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) - Version support
  - Delphi versions (7, XE-XE8, 10.x, 11 Alexandria, 12 Athens)
  - FreePascal versions (3.0, 3.2, 3.4)
  - Lazarus versions (2.x, 3.x)
  - Component libraries (FireDAC, IBX, Zeos, SQLdb)
  - Operating systems (Windows, Linux, macOS)
  - Architectures (32-bit, 64-bit)

- [x] [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Migration from Firebird
  - From Firebird + FireDAC
  - From Firebird + IBX
  - From Firebird + Zeos
  - From Firebird + dbExpress
  - From Firebird + FIBPlus
  - From InterBase applications
  - Connection string migration
  - SQL dialect differences
  - Transaction isolation mapping

- [x] [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation
  - FireDAC connection parameters
  - IBX component properties
  - Zeos connection settings
  - SQLdb configuration
  - Event handling
  - Blob streaming API

### Shared References

- [../DRIVER_BASELINE_SPEC.md](../DRIVER_BASELINE_SPEC.md) - Shared V2 driver requirements.

### Examples Directory

- [ ] **examples/firedac/** - FireDAC examples (Delphi)
  - **BasicConnection.dpr** - Simple FireDAC connection
  - **DatasetExample.dpr** - TFDQuery usage
  - **TransactionExample.dpr** - Transaction management
  - **BlobExample.dpr** - Blob field handling
  - **EventsExample.dpr** - Event notifications
  - **MasterDetail.dpr** - Master-detail relationships
- [ ] **examples/ibx/** - IBX compatibility
  - **IBXConnection.dpr** - IBX components
  - **IBXTransaction.dpr** - Transaction control
  - **IBXDataset.dpr** - Dataset navigation
- [ ] **examples/zeos/** - Zeos components
  - **ZeosConnection.dpr** - Zeos connection
  - **ZeosQuery.dpr** - TZQuery usage
- [ ] **examples/lazarus/** - Lazarus/FreePascal
  - **BasicSQLdb.lpr** - SQLdb components
  - **LazarusApp/** - Full Lazarus application
  - **DataModule.pas** - Data module pattern
- [ ] **examples/vcl-app/** - Full VCL application
  - **MainForm.pas** - Main form
  - **DataModule.pas** - Data access layer
  - **CustomerList.dfm** - Example form
- [ ] **examples/migration/** - Migration examples
  - **FirebirdToScratchBird.pas** - Migration utility
  - **ConnectionStringConverter.pas** - Connection conversion
  - **SQLDialectConverter.pas** - SQL syntax conversion

---

## Key Integration Points

### FireDAC (Fire Data Access Components)

**Critical**: FireDAC is Delphi's standard database access framework (Delphi XE5+).

Requirements:
- Custom FireDAC driver (TFDPhysScratchBirdDriverLink)
- Connection definition support
- Connection pooling
- Metadata support (TFDMetaInfoQuery)
- Schema caching
- Batch operations
- Array DML
- Local SQL engine integration

Example FireDAC usage:
```pascal
uses
  FireDAC.Comp.Client, FireDAC.Stan.Def, FireDAC.Phys.ScratchBird;

procedure TForm1.ConnectToScratchBird;
var
  Connection: TFDConnection;
  Query: TFDQuery;
begin
  Connection := TFDConnection.Create(nil);
  try
    // Connection parameters
    Connection.DriverName := 'ScratchBird';
    Connection.Params.Values['Server'] := 'localhost';
    Connection.Params.Values['Port'] := '3092';
    Connection.Params.Values['Database'] := 'mydb';
    Connection.Params.Values['User_Name'] := 'myuser';
    Connection.Params.Values['Password'] := 'mypass';
    Connection.Params.Values['Protocol'] := 'TCPIP';

    // Connect
    Connection.Connected := True;

    // Query example
    Query := TFDQuery.Create(nil);
    try
      Query.Connection := Connection;
      Query.SQL.Text := 'SELECT * FROM customers WHERE active = :active';
      Query.ParamByName('active').AsBoolean := True;
      Query.Open;

      while not Query.Eof do
      begin
        ShowMessage(Query.FieldByName('customer_name').AsString);
        Query.Next;
      end;
    finally
      Query.Free;
    end;
  finally
    Connection.Free;
  end;
end;

// Transaction example
procedure TForm1.ExecuteTransaction;
begin
  FDConnection1.StartTransaction;
  try
    FDQuery1.SQL.Text := 'INSERT INTO orders (customer_id, total) VALUES (:cust_id, :total)';
    FDQuery1.ParamByName('cust_id').AsInteger := 123;
    FDQuery1.ParamByName('total').AsCurrency := 99.99;
    FDQuery1.ExecSQL;

    FDQuery2.SQL.Text := 'UPDATE inventory SET stock = stock - :qty WHERE product_id = :prod_id';
    FDQuery2.ParamByName('qty').AsInteger := 1;
    FDQuery2.ParamByName('prod_id').AsInteger := 456;
    FDQuery2.ExecSQL;

    FDConnection1.Commit;
  except
    on E: Exception do
    begin
      FDConnection1.Rollback;
      raise;
    end;
  end;
end;
```

### IBX (InterBase Express) Compatibility

**Important**: Many Firebird applications use IBX components.

Requirements:
- IBX-compatible components (TIBDatabase, TIBTransaction, TIBQuery, etc.)
- Drop-in replacement for existing IBX code
- Firebird SQL dialect support
- Event alerter support
- Blob streaming
- Array field support

Example IBX compatibility:
```pascal
uses
  ScratchBird.IBX; // IBX-compatible components

procedure TDataModule1.DataModuleCreate(Sender: TObject);
begin
  // IBDatabase configuration (IBX-compatible)
  IBDatabase1.DatabaseName := 'localhost/3092:mydb';
  IBDatabase1.Params.Values['user_name'] := 'myuser';
  IBDatabase1.Params.Values['password'] := 'mypass';
  IBDatabase1.Params.Values['sql_dialect'] := '3';
  IBDatabase1.Connected := True;

  // Transaction
  IBTransaction1.DefaultDatabase := IBDatabase1;
  IBTransaction1.Params.Add('read_committed');
  IBTransaction1.Params.Add('rec_version');
  IBTransaction1.Params.Add('nowait');

  // Query
  IBQuery1.Database := IBDatabase1;
  IBQuery1.Transaction := IBTransaction1;
  IBQuery1.SQL.Text := 'SELECT * FROM products WHERE price > :min_price';
  IBQuery1.ParamByName('min_price').AsCurrency := 10.00;
  IBQuery1.Open;
end;

// Blob handling (Firebird-style)
procedure TForm1.LoadBlobToStream;
var
  BlobStream: TStream;
begin
  IBQuery1.Open;
  if not IBQuery1.Eof then
  begin
    BlobStream := IBQuery1.CreateBlobStream(IBQuery1.FieldByName('document'), bmRead);
    try
      Memo1.Lines.LoadFromStream(BlobStream);
    finally
      BlobStream.Free;
    end;
  end;
end;
```

### Zeos Database Components

**Important**: Popular open-source database components for Delphi/Lazarus.

Requirements:
- Zeos protocol driver (protocol 'scratchbird')
- TZConnection support
- TZQuery, TZReadOnlyQuery, TZTable support
- Metadata provider
- Connection pooling

Example Zeos usage:
```pascal
uses
  ZConnection, ZDataset;

procedure TForm1.FormCreate(Sender: TObject);
begin
  ZConnection1.Protocol := 'scratchbird';
  ZConnection1.HostName := 'localhost';
  ZConnection1.Port := 3092;
  ZConnection1.Database := 'mydb';
  ZConnection1.User := 'myuser';
  ZConnection1.Password := 'mypass';
  ZConnection1.Connected := True;

  ZQuery1.Connection := ZConnection1;
  ZQuery1.SQL.Text := 'SELECT * FROM employees';
  ZQuery1.Open;
end;
```

### FreePascal/Lazarus SQLdb Components

**Important**: Standard database access for FreePascal/Lazarus.

Requirements:
- TScratchBirdConnection component
- SQLdb connector registration
- TSQLQuery, TSQLTransaction support
- Cross-platform compatibility (Windows, Linux, macOS)

Example Lazarus/SQLdb usage:
```pascal
uses
  sqldb, scratchbird_sqldb;

procedure TDataModule1.DataModuleCreate(Sender: TObject);
begin
  ScratchBirdConnection1.HostName := 'localhost';
  ScratchBirdConnection1.DatabaseName := 'mydb';
  ScratchBirdConnection1.UserName := 'myuser';
  ScratchBirdConnection1.Password := 'mypass';
  ScratchBirdConnection1.Connected := True;

  SQLTransaction1.Database := ScratchBirdConnection1;
  SQLTransaction1.Active := True;

  SQLQuery1.Database := ScratchBirdConnection1;
  SQLQuery1.Transaction := SQLTransaction1;
  SQLQuery1.SQL.Text := 'SELECT * FROM customers';
  SQLQuery1.Open;
end;
```

### Firebird Migration Support

**Critical**: Tools and utilities to ease migration from Firebird.

Requirements:
- Connection string converter
- SQL dialect converter (Firebird → ScratchBird)
- Metadata migration (extracting from Firebird)
- Data pump utility
- Transaction isolation mapping

Example migration utility:
```pascal
unit FirebirdMigration;

interface

type
  TFirebirdMigrator = class
  private
    FSourceConnection: TFDConnection; // Firebird
    FTargetConnection: TFDConnection; // ScratchBird
  public
    function ConvertConnectionString(const FirebirdConnStr: string): string;
    function ConvertSQL(const FirebirdSQL: string): string;
    procedure MigrateSchema;
    procedure MigrateData(const TableName: string);
    procedure MigrateAll;
  end;

implementation

function TFirebirdMigrator.ConvertConnectionString(const FirebirdConnStr: string): string;
begin
  // Convert Firebird connection string to ScratchBird format
  // Example: 'localhost:C:\data\mydb.fdb' → 'localhost:3092:mydb'
  Result := ...;
end;

function TFirebirdMigrator.ConvertSQL(const FirebirdSQL: string): string;
begin
  // Convert Firebird-specific SQL to ScratchBird
  // Handle: FIRST/SKIP, PLAN clauses, dialect differences, etc.
  Result := ...;
end;

procedure TFirebirdMigrator.MigrateSchema;
var
  MetaQuery: TFDQuery;
  DDL: TStringList;
begin
  // Extract Firebird schema
  MetaQuery := TFDQuery.Create(nil);
  DDL := TStringList.Create;
  try
    // Get tables, indexes, constraints, triggers, procedures
    // Convert to ScratchBird DDL
    // Execute on target
  finally
    MetaQuery.Free;
    DDL.Free;
  end;
end;
```

---

## Type Mappings

### Delphi ↔ ScratchBird Type Mappings

| Delphi/Pascal Type | ScratchBird SQL Type | Notes |
|-------------------|----------------------|-------|
| Integer | INTEGER | 32-bit signed |
| Int64 | BIGINT | 64-bit signed |
| SmallInt | SMALLINT | 16-bit signed |
| Word | SMALLINT | 16-bit unsigned (check range) |
| Cardinal | INTEGER | 32-bit unsigned (check range) |
| Single | REAL | 32-bit float |
| Double | DOUBLE PRECISION | 64-bit float |
| Currency | DECIMAL(19,4) | Fixed-point |
| Boolean | BOOLEAN | True/False |
| String | VARCHAR(n) | Variable length |
| AnsiString | VARCHAR(n) | ANSI encoding |
| WideString | VARCHAR(n) | Unicode |
| UnicodeString | VARCHAR(n) | Unicode (Delphi 2009+) |
| TDateTime | TIMESTAMP | Date and time |
| TDate | DATE | Date only |
| TTime | TIME | Time only |
| TBytes | BYTEA | Binary data |
| TBlobField | BLOB | Large binary |
| TMemoField | TEXT | Large text |
| TGUID | UUID | 128-bit UUID |

### Firebird Compatibility Types

| Firebird Type | ScratchBird Type | Notes |
|--------------|------------------|-------|
| INTEGER | INTEGER | Direct mapping |
| BIGINT | BIGINT | Direct mapping |
| SMALLINT | SMALLINT | Direct mapping |
| FLOAT | REAL | 32-bit |
| DOUBLE PRECISION | DOUBLE PRECISION | 64-bit |
| NUMERIC(p,s) | NUMERIC(p,s) | Fixed precision |
| DECIMAL(p,s) | DECIMAL(p,s) | Fixed precision |
| CHAR(n) | CHAR(n) | Fixed length |
| VARCHAR(n) | VARCHAR(n) | Variable length |
| BLOB SUB_TYPE TEXT | TEXT | Large text |
| BLOB SUB_TYPE BINARY | BYTEA | Large binary |
| DATE | DATE | Date only |
| TIME | TIME | Time only |
| TIMESTAMP | TIMESTAMP | Date and time |
| BOOLEAN | BOOLEAN | True/False (FB 3.0+) |

---

## Performance Targets

Benchmark against Firebird with FireDAC:

| Operation | Target Performance |
|-----------|-------------------|
| Connection establishment | Within 10% of Firebird |
| Simple SELECT (1 row) | Within 10% of Firebird |
| Bulk SELECT (10,000 rows) | Within 15% of Firebird |
| Parameterized INSERT | Within 10% of Firebird |
| Batch INSERT (1,000 rows) | Within 15% of Firebird |
| Transaction commit | Within 10% of Firebird |
| Blob streaming | Within 15% of Firebird |
| FireDAC metadata retrieval | Within 20% of Firebird |

---

## Dependencies

### Delphi Requirements

- Delphi 7 or later (for legacy support)
- Delphi XE or later (recommended)
- Delphi 10.4 Sydney or later (for modern FireDAC)
- FireDAC (included in Delphi XE5+)

### FreePascal/Lazarus Requirements

- FreePascal 3.0 or later
- Lazarus 2.0 or later
- SQLdb components (included)

### Build Requirements

- Delphi IDE or FreePascal compiler
- Optional: Visual Studio (for Windows DLL components)

---

## Installation

### FireDAC Driver Installation (Delphi)

```
1. Download ScratchBirdFireDAC.bpl (Design-time package)
2. Download ScratchBirdFireDACRuntime.bpl (Runtime package)
3. Install design-time package in Delphi IDE:
   Component → Install Packages → Add → Select ScratchBirdFireDAC.bpl
4. Add runtime package path to Library Path
5. TFDPhysScratchBirdDriverLink component now available in Tool Palette
```

### FreePascal/Lazarus Installation

```bash
# Install from Lazarus package
cd /path/to/scratchbird-pascal
lazbuild scratchbird_lazarus.lpk

# Or install manually
fpc -Fu/path/to/scratchbird-pascal/src yourapp.pas
```

### Source Installation

```
1. Clone repository: git clone https://github.com/scratchbird/scratchbird-pascal.git
2. Add source path to Delphi Library Path or FreePascal unit search path
3. Add uses clause: ScratchBird.FireDAC or ScratchBird.SQLdb
```

---

## Documentation Requirements

### User Documentation

- [ ] Quick start guide (Delphi and Lazarus)
- [ ] Installation instructions
- [ ] Connection string format
- [ ] FireDAC driver configuration
- [ ] IBX compatibility guide
- [ ] Transaction management (Firebird-style)
- [ ] Blob handling guide
- [ ] Event notification guide
- [ ] Performance tuning tips
- [ ] Firebird migration guide (CRITICAL)

### API Documentation

- [ ] Complete component reference (FireDAC, IBX, Zeos, SQLdb)
- [ ] Property reference
- [ ] Method reference
- [ ] Event reference
- [ ] Type mapping reference

### Migration Documentation

- [ ] Firebird to ScratchBird migration guide
- [ ] Connection string migration
- [ ] SQL dialect differences
- [ ] Transaction isolation mapping
- [ ] Stored procedure migration
- [ ] Trigger migration
- [ ] Index migration

---

## Release Criteria

### Functional Completeness

- [ ] FireDAC driver working (Delphi 10.4+)
- [ ] IBX compatibility layer working
- [ ] Zeos components support
- [ ] FreePascal/Lazarus SQLdb support
- [ ] All standard Delphi types supported
- [ ] Transaction management (Firebird-compatible)
- [ ] Blob streaming functional
- [ ] Event notifications working
- [ ] Metadata support complete

### Quality Metrics

- [ ] >90% test coverage
- [ ] FireDAC test suite passing
- [ ] IBX compatibility tests passing
- [ ] Performance benchmarks met (within 10-15% of Firebird)
- [ ] Firebird migration successful (10+ real applications)
- [ ] Memory leak tests passing

### Documentation

- [ ] Complete API reference
- [ ] User guide complete
- [ ] 15+ code examples (Delphi and Lazarus)
- [ ] Firebird migration guide with examples
- [ ] Troubleshooting guide

### Packaging

- [ ] Delphi BPL packages (design-time and runtime)
- [ ] FreePascal LPK package
- [ ] Source distribution
- [ ] GetIt package manager (Delphi)
- [ ] Online Package Manager (Lazarus)

---

## Community and Support

### Package Locations

- GitHub: https://github.com/scratchbird/scratchbird-pascal
- GetIt (Delphi): Search "ScratchBird" in GetIt Package Manager
- Lazarus OPM: https://packages.lazarus-ide.org/ (search: scratchbird)
- Documentation: https://scratchbird.dev/docs/pascal/

### Issue Tracking

- GitHub Issues: https://github.com/scratchbird/scratchbird-pascal/issues
- Response time target: <3 days
- Bug fix SLA: Critical <7 days, High <14 days

### Community Channels

- Discord: ScratchBird #pascal-delphi
- Delphi-PRAXiS forum: ScratchBird subforum
- Lazarus forum: ScratchBird thread
- Stack Overflow: Tag `scratchbird-delphi` or `scratchbird-pascal`

---

## References

1. **FireDAC Documentation**
   https://docwiki.embarcadero.com/RADStudio/en/FireDAC

2. **IBX Documentation**
   http://www.ibphoenix.com/resources/documents/design/doc_18

3. **Zeos Documentation**
   https://zeoslib.sourceforge.io/

4. **FreePascal SQLdb**
   https://wiki.freepascal.org/SqlDB

5. **Firebird Documentation**
   https://firebirdsql.org/en/documentation/

---

## Next Steps

1. **Create detailed SPECIFICATION.md** with complete FireDAC driver API
2. **Develop IMPLEMENTATION_PLAN.md** with milestones and timeline
3. **Write TESTING_CRITERIA.md** with FireDAC/IBX compliance tests
4. **Begin prototype** with basic FireDAC driver
5. **Firebird migration toolkit** (connection converter, SQL converter, data pump)
6. **Community outreach** to Firebird/Delphi communities

---

**Document Version:** 1.0 (Template)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
**Strategic Priority:** CRITICAL for Firebird user migration
