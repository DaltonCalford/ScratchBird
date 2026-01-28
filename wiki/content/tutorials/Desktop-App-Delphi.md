# Desktop App: Delphi

**Last Updated:** 2026-01-28

---

## Overview

This tutorial guides you through building a desktop application with Delphi/RAD Studio, connecting to ScratchBird using FireDAC. You'll create a task management application with a visual interface using VCL components.

**What you'll build:**
- A Windows desktop application
- Database connection using FireDAC
- Data-aware forms for CRUD operations
- Master-detail relationships

**Time:** 60-90 minutes

---

## Prerequisites

- Delphi 10.4+ or RAD Studio
- ScratchBird running with Firebird protocol enabled (port 3050) or PostgreSQL (port 5432)
- Basic Delphi knowledge

---

## Connection Options

ScratchBird supports multiple protocols. For Delphi applications:

| Protocol | Port | FireDAC Driver | Best For |
|----------|------|----------------|----------|
| Firebird | 3050 | `FB` | Existing Firebird apps, IBX migration |
| PostgreSQL | 5432 | `PG` | New applications, broader compatibility |

This tutorial covers both approaches.

---

## Part 1: Project Setup

### Create New VCL Application

1. File > New > VCL Forms Application - Delphi
2. Save project as `TaskManager`
3. Save main form unit as `MainForm.pas`

### Add Required Components

From Tool Palette, add to the form:
- **Data Access**: TFDConnection, TFDQuery (x3), TDataSource (x3)
- **DB Controls**: TDBGrid (x2), TDBNavigator, TDBEdit, TDBMemo, TDBComboBox
- **Standard**: TPanel, TButton, TSplitter, TLabel

---

## Part 2: Database Connection (Firebird Protocol)

### Configure TFDConnection

Select `FDConnection1` and set properties:

```
DriverName = FB
```

Double-click to open Connection Editor, then set:

| Parameter | Value |
|-----------|-------|
| Server | localhost |
| Port | 3050 |
| Database | taskmanager |
| User_Name | admin |
| Password | (your password) |
| CharacterSet | UTF8 |

### Or Configure via Code

```pascal
procedure TMainForm.ConfigureConnection;
begin
  FDConnection1.DriverName := 'FB';
  FDConnection1.Params.Clear;
  FDConnection1.Params.Add('Server=localhost');
  FDConnection1.Params.Add('Port=3050');
  FDConnection1.Params.Add('Database=taskmanager');
  FDConnection1.Params.Add('User_Name=admin');
  FDConnection1.Params.Add('Password=secret');
  FDConnection1.Params.Add('CharacterSet=UTF8');
end;
```

---

## Part 3: Database Connection (PostgreSQL Protocol)

### Configure TFDConnection for PostgreSQL

```
DriverName = PG
```

Connection parameters:

| Parameter | Value |
|-----------|-------|
| Server | localhost |
| Port | 5432 |
| Database | taskmanager |
| User_Name | admin |
| Password | (your password) |

### Via Code

```pascal
procedure TMainForm.ConfigureConnectionPG;
begin
  FDConnection1.DriverName := 'PG';
  FDConnection1.Params.Clear;
  FDConnection1.Params.Add('Server=localhost');
  FDConnection1.Params.Add('Port=5432');
  FDConnection1.Params.Add('Database=taskmanager');
  FDConnection1.Params.Add('User_Name=admin');
  FDConnection1.Params.Add('Password=secret');
end;
```

---

## Part 4: Database Schema

Create the schema in ScratchBird (if not already done):

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    email VARCHAR(100) NOT NULL UNIQUE,
    full_name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE projects (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    owner_id INTEGER NOT NULL REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'active',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE tasks (
    id SERIAL PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    description TEXT,
    project_id INTEGER NOT NULL REFERENCES projects(id),
    assigned_to INTEGER REFERENCES users(id),
    priority INTEGER DEFAULT 3,
    status VARCHAR(20) DEFAULT 'pending',
    due_date DATE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert sample data
INSERT INTO users (username, email, full_name) VALUES
    ('admin', 'admin@example.com', 'Administrator');

INSERT INTO projects (name, description, owner_id) VALUES
    ('Sample Project', 'A sample project for testing', 1);
```

---

## Part 5: Configure Queries

### Projects Query (FDQuery1)

Set properties:
```
Connection = FDConnection1
SQL.Text = SELECT * FROM projects ORDER BY name
```

### Tasks Query (FDQuery2)

For master-detail relationship:
```
Connection = FDConnection1
MasterSource = DataSource1
MasterFields = id
DetailFields = project_id
SQL.Text = SELECT t.*, u.full_name as assignee_name
           FROM tasks t
           LEFT JOIN users u ON t.assigned_to = u.id
           WHERE t.project_id = :id
           ORDER BY t.priority, t.due_date
```

### Users Query (FDQuery3)

For combo box lookups:
```
Connection = FDConnection1
SQL.Text = SELECT id, full_name FROM users ORDER BY full_name
```

### Connect DataSources

| DataSource | DataSet |
|------------|---------|
| DataSource1 | FDQuery1 (Projects) |
| DataSource2 | FDQuery2 (Tasks) |
| DataSource3 | FDQuery3 (Users) |

---

## Part 6: Form Layout

### Design the Interface

```
+----------------------------------------------------------+
| [Connect] [Disconnect]                    Task Manager    |
+----------------------------------------------------------+
| Projects                    | Tasks                       |
| +------------------------+  | +-------------------------+ |
| | DBGrid1 (Projects)     |  | | DBGrid2 (Tasks)         | |
| |                        |  | |                         | |
| |                        |  | |                         | |
| +------------------------+  | +-------------------------+ |
| [Add] [Edit] [Delete]       | [Add] [Edit] [Delete]       |
+----------------------------------------------------------+
| Task Details                                              |
| Title: [_______________]  Priority: [___]                 |
| Description: [________________]  Status: [________]       |
| Due Date: [__________]  Assigned To: [____________]       |
| [Save] [Cancel]                                           |
+----------------------------------------------------------+
```

### Configure DBGrids

**DBGrid1 (Projects):**
- DataSource = DataSource1
- Columns: name, status, created_at

**DBGrid2 (Tasks):**
- DataSource = DataSource2
- Columns: title, priority, status, due_date, assignee_name

---

## Part 7: Main Form Code

### Unit Declaration

```pascal
unit MainForm;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants,
  System.Classes, Vcl.Graphics, Vcl.Controls, Vcl.Forms, Vcl.Dialogs,
  FireDAC.Stan.Intf, FireDAC.Stan.Option, FireDAC.Stan.Error,
  FireDAC.UI.Intf, FireDAC.Phys.Intf, FireDAC.Stan.Def,
  FireDAC.Stan.Pool, FireDAC.Stan.Async, FireDAC.Phys,
  FireDAC.Phys.FB, FireDAC.Phys.FBDef, // For Firebird
  FireDAC.Phys.PG, FireDAC.Phys.PGDef, // For PostgreSQL
  FireDAC.VCLUI.Wait, FireDAC.Comp.Client,
  Data.DB, Vcl.Grids, Vcl.DBGrids, Vcl.StdCtrls, Vcl.ExtCtrls,
  Vcl.DBCtrls, FireDAC.Stan.Param, FireDAC.DatS, FireDAC.DApt.Intf,
  FireDAC.DApt, FireDAC.Comp.DataSet;

type
  TfrmMain = class(TForm)
    FDConnection1: TFDConnection;
    FDQuery1: TFDQuery;      // Projects
    FDQuery2: TFDQuery;      // Tasks
    FDQuery3: TFDQuery;      // Users
    DataSource1: TDataSource;
    DataSource2: TDataSource;
    DataSource3: TDataSource;
    pnlTop: TPanel;
    btnConnect: TButton;
    btnDisconnect: TButton;
    pnlMain: TPanel;
    pnlProjects: TPanel;
    pnlTasks: TPanel;
    DBGrid1: TDBGrid;
    DBGrid2: TDBGrid;
    btnAddProject: TButton;
    btnEditProject: TButton;
    btnDeleteProject: TButton;
    btnAddTask: TButton;
    btnEditTask: TButton;
    btnDeleteTask: TButton;
    pnlDetails: TPanel;
    lblTitle: TLabel;
    edtTitle: TDBEdit;
    lblPriority: TLabel;
    edtPriority: TDBEdit;
    lblDescription: TLabel;
    mmoDescription: TDBMemo;
    lblStatus: TLabel;
    cboStatus: TDBComboBox;
    lblDueDate: TLabel;
    edtDueDate: TDBEdit;
    lblAssignedTo: TLabel;
    cboAssignedTo: TDBLookupComboBox;
    btnSave: TButton;
    btnCancel: TButton;
    procedure FormCreate(Sender: TObject);
    procedure btnConnectClick(Sender: TObject);
    procedure btnDisconnectClick(Sender: TObject);
    procedure btnAddProjectClick(Sender: TObject);
    procedure btnEditProjectClick(Sender: TObject);
    procedure btnDeleteProjectClick(Sender: TObject);
    procedure btnAddTaskClick(Sender: TObject);
    procedure btnEditTaskClick(Sender: TObject);
    procedure btnDeleteTaskClick(Sender: TObject);
    procedure btnSaveClick(Sender: TObject);
    procedure btnCancelClick(Sender: TObject);
  private
    procedure UpdateUI;
    procedure OpenQueries;
    procedure CloseQueries;
  public
  end;

var
  frmMain: TfrmMain;

implementation

{$R *.dfm}
```

### Form Creation

```pascal
procedure TfrmMain.FormCreate(Sender: TObject);
begin
  // Configure connection (choose one)
  FDConnection1.DriverName := 'PG';  // Or 'FB' for Firebird
  FDConnection1.Params.Clear;
  FDConnection1.Params.Add('Server=localhost');
  FDConnection1.Params.Add('Port=5432');  // Or 3050 for Firebird
  FDConnection1.Params.Add('Database=taskmanager');
  FDConnection1.Params.Add('User_Name=admin');
  FDConnection1.Params.Add('Password=secret');

  // Configure status combo box
  cboStatus.Items.Clear;
  cboStatus.Items.Add('pending');
  cboStatus.Items.Add('in_progress');
  cboStatus.Items.Add('completed');
  cboStatus.Items.Add('cancelled');

  // Configure assigned to lookup
  cboAssignedTo.ListSource := DataSource3;
  cboAssignedTo.ListField := 'full_name';
  cboAssignedTo.KeyField := 'id';
  cboAssignedTo.DataSource := DataSource2;
  cboAssignedTo.DataField := 'assigned_to';

  UpdateUI;
end;
```

### Connection Handling

```pascal
procedure TfrmMain.btnConnectClick(Sender: TObject);
begin
  try
    FDConnection1.Connected := True;
    OpenQueries;
    UpdateUI;
    ShowMessage('Connected successfully');
  except
    on E: Exception do
      ShowMessage('Connection failed: ' + E.Message);
  end;
end;

procedure TfrmMain.btnDisconnectClick(Sender: TObject);
begin
  CloseQueries;
  FDConnection1.Connected := False;
  UpdateUI;
end;

procedure TfrmMain.OpenQueries;
begin
  FDQuery3.Open;  // Users first (for lookups)
  FDQuery1.Open;  // Projects
  FDQuery2.Open;  // Tasks (depends on Projects)
end;

procedure TfrmMain.CloseQueries;
begin
  FDQuery2.Close;
  FDQuery1.Close;
  FDQuery3.Close;
end;

procedure TfrmMain.UpdateUI;
var
  Connected: Boolean;
begin
  Connected := FDConnection1.Connected;
  btnConnect.Enabled := not Connected;
  btnDisconnect.Enabled := Connected;
  btnAddProject.Enabled := Connected;
  btnEditProject.Enabled := Connected and not FDQuery1.IsEmpty;
  btnDeleteProject.Enabled := Connected and not FDQuery1.IsEmpty;
  btnAddTask.Enabled := Connected and not FDQuery1.IsEmpty;
  btnEditTask.Enabled := Connected and not FDQuery2.IsEmpty;
  btnDeleteTask.Enabled := Connected and not FDQuery2.IsEmpty;
  pnlDetails.Visible := Connected and (FDQuery2.State in [dsEdit, dsInsert]);
end;
```

### Project CRUD Operations

```pascal
procedure TfrmMain.btnAddProjectClick(Sender: TObject);
var
  ProjectName: string;
begin
  ProjectName := InputBox('New Project', 'Enter project name:', '');
  if ProjectName <> '' then
  begin
    FDConnection1.ExecSQL(
      'INSERT INTO projects (name, owner_id) VALUES (:name, 1)',
      [ProjectName]);
    FDQuery1.Refresh;
  end;
end;

procedure TfrmMain.btnEditProjectClick(Sender: TObject);
var
  ProjectName: string;
begin
  if FDQuery1.IsEmpty then Exit;

  ProjectName := InputBox('Edit Project', 'Enter project name:',
    FDQuery1.FieldByName('name').AsString);

  if ProjectName <> '' then
  begin
    FDConnection1.ExecSQL(
      'UPDATE projects SET name = :name WHERE id = :id',
      [ProjectName, FDQuery1.FieldByName('id').AsInteger]);
    FDQuery1.Refresh;
  end;
end;

procedure TfrmMain.btnDeleteProjectClick(Sender: TObject);
begin
  if FDQuery1.IsEmpty then Exit;

  if MessageDlg('Delete this project and all its tasks?',
    mtConfirmation, [mbYes, mbNo], 0) = mrYes then
  begin
    FDConnection1.ExecSQL(
      'DELETE FROM projects WHERE id = :id',
      [FDQuery1.FieldByName('id').AsInteger]);
    FDQuery1.Refresh;
  end;
end;
```

### Task CRUD Operations

```pascal
procedure TfrmMain.btnAddTaskClick(Sender: TObject);
begin
  if FDQuery1.IsEmpty then Exit;

  FDQuery2.Append;
  FDQuery2.FieldByName('project_id').AsInteger :=
    FDQuery1.FieldByName('id').AsInteger;
  FDQuery2.FieldByName('priority').AsInteger := 3;
  FDQuery2.FieldByName('status').AsString := 'pending';
  pnlDetails.Visible := True;
  edtTitle.SetFocus;
  UpdateUI;
end;

procedure TfrmMain.btnEditTaskClick(Sender: TObject);
begin
  if FDQuery2.IsEmpty then Exit;

  FDQuery2.Edit;
  pnlDetails.Visible := True;
  edtTitle.SetFocus;
  UpdateUI;
end;

procedure TfrmMain.btnDeleteTaskClick(Sender: TObject);
begin
  if FDQuery2.IsEmpty then Exit;

  if MessageDlg('Delete this task?',
    mtConfirmation, [mbYes, mbNo], 0) = mrYes then
  begin
    FDQuery2.Delete;
  end;
end;

procedure TfrmMain.btnSaveClick(Sender: TObject);
begin
  try
    FDQuery2.Post;
    pnlDetails.Visible := False;
    UpdateUI;
  except
    on E: Exception do
      ShowMessage('Save failed: ' + E.Message);
  end;
end;

procedure TfrmMain.btnCancelClick(Sender: TObject);
begin
  FDQuery2.Cancel;
  pnlDetails.Visible := False;
  UpdateUI;
end;
```

---

## Part 8: Advanced Features

### Using Transactions

```pascal
procedure TfrmMain.CreateProjectWithTasks(const ProjectName: string;
  const TaskTitles: array of string);
var
  ProjectID: Integer;
  Title: string;
begin
  FDConnection1.StartTransaction;
  try
    // Create project
    FDConnection1.ExecSQL(
      'INSERT INTO projects (name, owner_id) VALUES (:name, 1) RETURNING id',
      [ProjectName]);

    // Get the new project ID
    ProjectID := FDConnection1.ExecSQLScalar(
      'SELECT MAX(id) FROM projects WHERE name = :name',
      [ProjectName]);

    // Create tasks
    for Title in TaskTitles do
    begin
      FDConnection1.ExecSQL(
        'INSERT INTO tasks (title, project_id, priority) VALUES (:title, :pid, 3)',
        [Title, ProjectID]);
    end;

    FDConnection1.Commit;
    FDQuery1.Refresh;
    FDQuery2.Refresh;
  except
    FDConnection1.Rollback;
    raise;
  end;
end;
```

### Filtering Tasks

```pascal
procedure TfrmMain.FilterTasks(const Status: string);
begin
  if Status = '' then
    FDQuery2.Filter := ''
  else
    FDQuery2.Filter := Format('status = ''%s''', [Status]);

  FDQuery2.Filtered := Status <> '';
end;

procedure TfrmMain.ShowOverdueTasks;
begin
  FDQuery2.Close;
  FDQuery2.SQL.Text :=
    'SELECT t.*, u.full_name as assignee_name ' +
    'FROM tasks t ' +
    'LEFT JOIN users u ON t.assigned_to = u.id ' +
    'WHERE t.due_date < CURRENT_DATE ' +
    'AND t.status NOT IN (''completed'', ''cancelled'') ' +
    'ORDER BY t.due_date';
  FDQuery2.Open;
end;
```

### Task Statistics

```pascal
function TfrmMain.GetTaskStats: string;
var
  Query: TFDQuery;
begin
  Query := TFDQuery.Create(nil);
  try
    Query.Connection := FDConnection1;
    Query.SQL.Text :=
      'SELECT ' +
      '  COUNT(*) as total, ' +
      '  COUNT(CASE WHEN status = ''pending'' THEN 1 END) as pending, ' +
      '  COUNT(CASE WHEN status = ''in_progress'' THEN 1 END) as in_progress, ' +
      '  COUNT(CASE WHEN status = ''completed'' THEN 1 END) as completed ' +
      'FROM tasks';
    Query.Open;

    Result := Format('Total: %d, Pending: %d, In Progress: %d, Completed: %d',
      [Query.FieldByName('total').AsInteger,
       Query.FieldByName('pending').AsInteger,
       Query.FieldByName('in_progress').AsInteger,
       Query.FieldByName('completed').AsInteger]);
  finally
    Query.Free;
  end;
end;
```

### Connection String from INI File

```pascal
procedure TfrmMain.LoadConnectionFromIni;
var
  Ini: TIniFile;
begin
  Ini := TIniFile.Create(ExtractFilePath(Application.ExeName) + 'config.ini');
  try
    FDConnection1.DriverName := Ini.ReadString('Database', 'Driver', 'PG');
    FDConnection1.Params.Clear;
    FDConnection1.Params.Add('Server=' + Ini.ReadString('Database', 'Server', 'localhost'));
    FDConnection1.Params.Add('Port=' + Ini.ReadString('Database', 'Port', '5432'));
    FDConnection1.Params.Add('Database=' + Ini.ReadString('Database', 'Database', 'taskmanager'));
    FDConnection1.Params.Add('User_Name=' + Ini.ReadString('Database', 'User', 'admin'));
    FDConnection1.Params.Add('Password=' + Ini.ReadString('Database', 'Password', ''));
  finally
    Ini.Free;
  end;
end;
```

Example `config.ini`:
```ini
[Database]
Driver=PG
Server=localhost
Port=5432
Database=taskmanager
User=admin
Password=secret
```

---

## Part 9: Error Handling

### Connection Error Handling

```pascal
procedure TfrmMain.SafeConnect;
begin
  try
    Screen.Cursor := crHourGlass;
    try
      FDConnection1.Connected := True;
      OpenQueries;
    finally
      Screen.Cursor := crDefault;
    end;
  except
    on E: EFDDBEngineException do
    begin
      case E.Kind of
        ekServerGone:
          ShowMessage('Cannot connect to server. Please check if ScratchBird is running.');
        ekUserPwdInvalid:
          ShowMessage('Invalid username or password.');
        ekObjNotExists:
          ShowMessage('Database does not exist.');
        else
          ShowMessage('Database error: ' + E.Message);
      end;
    end;
    on E: Exception do
      ShowMessage('Connection error: ' + E.Message);
  end;
  UpdateUI;
end;
```

### Query Error Handling

```pascal
procedure TfrmMain.SafePost;
begin
  try
    FDQuery2.Post;
  except
    on E: EFDDBEngineException do
    begin
      if Pos('violates foreign key', E.Message) > 0 then
        ShowMessage('Invalid reference. Please check your selections.')
      else if Pos('violates unique', E.Message) > 0 then
        ShowMessage('This record already exists.')
      else if Pos('violates check', E.Message) > 0 then
        ShowMessage('Invalid value. Please check priority (1-5) and status.')
      else
        ShowMessage('Save failed: ' + E.Message);
      FDQuery2.Cancel;
    end;
  end;
end;
```

---

## Part 10: Deployment

### Required Files

For deployment, include:

1. **Application executable** (`TaskManager.exe`)
2. **Configuration file** (`config.ini`)
3. **FireDAC driver DLLs** (from RAD Studio redist):
   - For PostgreSQL: `libpq.dll`, `libeay32.dll`, `ssleay32.dll`
   - For Firebird: `fbclient.dll`

### FireDAC Deployment

Add to your uses clause:
```pascal
uses
  FireDAC.Phys.PGDef,  // PostgreSQL driver definition
  FireDAC.Phys.PG,     // PostgreSQL driver
  // Or for Firebird:
  FireDAC.Phys.FBDef,
  FireDAC.Phys.FB;
```

### Creating Installer

Use Inno Setup or similar:

```iss
[Files]
Source: "TaskManager.exe"; DestDir: "{app}"
Source: "config.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist
Source: "libpq.dll"; DestDir: "{app}"
Source: "libeay32.dll"; DestDir: "{app}"
Source: "ssleay32.dll"; DestDir: "{app}"
```

---

## Complete Form DFM

```dfm
object frmMain: TfrmMain
  Left = 0
  Top = 0
  Caption = 'Task Manager - ScratchBird'
  ClientHeight = 600
  ClientWidth = 900
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Segoe UI'
  Font.Style = []
  OnCreate = FormCreate

  object pnlTop: TPanel
    Left = 0
    Top = 0
    Width = 900
    Height = 41
    Align = alTop
    TabOrder = 0
    object btnConnect: TButton
      Left = 8
      Top = 8
      Width = 75
      Height = 25
      Caption = 'Connect'
      TabOrder = 0
      OnClick = btnConnectClick
    end
    object btnDisconnect: TButton
      Left = 89
      Top = 8
      Width = 75
      Height = 25
      Caption = 'Disconnect'
      TabOrder = 1
      OnClick = btnDisconnectClick
    end
  end

  // ... additional components
end
```

---

## Troubleshooting

### "Driver not found"
Ensure FireDAC driver units are in uses clause and driver DLLs are available.

### "Cannot connect to server"
Verify ScratchBird is running and the correct port is configured (3050 for Firebird, 5432 for PostgreSQL).

### "Invalid password"
Check credentials in connection parameters or config.ini.

### "Character set not found"
For Firebird protocol, ensure `CharacterSet=UTF8` is set.

---

## See Also

- [Delphi Driver Documentation](../drivers/Delphi.md)
- [First Application](First-Application.md)
- [Firebird Language Guide](../language-guides/firebirdsql/README.md)
- [PostgreSQL Language Guide](../language-guides/postgresql/README.md)

