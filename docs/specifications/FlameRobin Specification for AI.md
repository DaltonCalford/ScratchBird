# **Functional Specification for the Recreation of a Lightweight Database Administration Tool**

## **1\. Architectural Philosophy and Core Design Patterns**

The objective of this specification is to detail the functional and architectural requirements for creating a database administration tool that replicates the user experience, performance profile, and feature set of FlameRobin.1 While the original reference implementation targets the Firebird RDBMS, this specification abstracts those concepts to guide the development of an equivalent tool for an alternative database engine (e.g., PostgreSQL, SQLite, MariaDB). The resulting application must adhere to the core tenets of being lightweight, cross-platform, and dependency-minimal, utilizing native widget sets to ensure a small footprint and high responsiveness.2

### **1.1 The Single Document Interface (SDI) Paradigm**

A defining characteristic of the target application's user interface architecture is the strict adherence to the Single Document Interface (SDI) model, rejecting the Multiple Document Interface (MDI) often found in Windows-centric database tools.

In this model, the application does not contain a single "parent" window that clips and confines child windows (such as SQL editors or property pages). Instead, every major functional component—the main tree view, an SQL editor session, a table property inspector—must exist as a top-level operating system window.2 This design choice is critical for ensuring seamless cross-platform behavior, particularly on Linux and macOS window managers where MDI implementations are often substandard or visually incoherent.

The implication for the recreation is that the application must maintain a global "Window Manager" singleton internally. This manager tracks all open windows associated with a specific database connection. When the user disconnects from a database or the application shuts down, this manager is responsible for gracefully closing all independent windows associated with that session. This decoupling allows users to spread their workspace across multiple monitors easily, a workflow that MDI restricts.

### **1.2 The Database Object Hierarchy (DBH) and the Observer Pattern**

The central nervous system of the application is the Database Object Hierarchy (DBH). This is an in-memory object graph that represents the current state of the database connection. It is not merely a cache; it is the active model against which all User Interface (UI) components bind. To replicate the responsiveness of FlameRobin, the implementation must heavily utilize the **Observer Pattern**.2

In typical database tools, making a schema change (like adding a column) often requires a manual "Refresh" of the tree view to see the change. The target application must automate this.

**The Notification Cycle:**

1. **Action:** A user executes an ALTER TABLE statement in the SQL Editor window.  
2. **Parsing:** The application’s internal SQL parser identifies the statement type (DDL) and the target object (Table X).  
3. **State Update:** The SQL Editor (the subject) notifies the DBH Node corresponding to Table X.  
4. **Propagation:** The Table X node updates its internal metadata by querying the system catalogs.  
5. **Observation:** The Table X node then broadcasts a notification to all registered observers.  
   * *Observer A (Tree View):* Expands the node to show the new column in the hierarchy.  
   * *Observer B (Property Page):* Refreshes the HTML view to list the new column in the schema table.  
   * *Observer C (Autocomplete Cache):* Adds the new column name to the suggestion list for future queries.

This architecture ensures that the user interface never displays stale data regarding the database structure, creating a sensation of "liveness" that distinguishes the tool from static query runners.2

### **1.3 Threading Model and Non-Blocking Operations**

To maintain the "lightweight" feel, the main UI thread must never be blocked by network I/O or database processing. This requires a granular threading model where specific long-running actions are offloaded to worker threads.

| Operation | Threading Strategy | User Cancellation Support |
| :---- | :---- | :---- |
| **Connection Establishment** | Background Thread | Yes (Cancel Button on Progress Dialog) 3 |
| **Metadata Loading** | Main Thread (Lazy) or Background | No (Fast Operation) |
| **SQL Execution (DML/DDL)** | Background Worker Thread | Yes (Terminates Query) |
| **Data Grid Fetching** | Background Worker Thread | Yes (Stop Fetching) 3 |
| **Backup/Restore** | Dedicated Service Thread | Yes (Aborts Service Call) 2 |

**Implementation Requirement:** The application must implement a thread-safe communication channel (e.g., event posting) to update the UI (like the progress bar or log window) from these background threads without causing race conditions or UI freezes.

## ---

**2\. System Configuration and Environment**

The application must be fully portable, storing its configuration in a manner that supports both per-user isolation and easy deployment.

### **2.1 Configuration Persistence**

The state of the application is persisted in two primary human-readable text files, typically located in the user’s home directory or a location specified by environment variables.

* **fr\_databases.conf**: This file acts as the registry for all known servers and databases. It must store a hierarchical tree structure of:  
  * Root  
  * Server Group (optional logical grouping)  
  * Server (Hostname/Port)  
  * Database (Path/Alias, Authentication Method, saved credentials if opted).  
  * *Requirement:* Passwords must not be stored in plain text; they should be obfuscated or encrypted using a machine-specific key.4  
* **fr\_settings.conf**: This file stores global application preferences and window states.  
  * *Window Metrics:* The application must remember the size and screen position of the Main Window, SQL Editors, and Property Pages between sessions.5  
  * *Editor Styles:* Syntax highlighting colors, font choices, and auto-completion behavior.  
  * *History Settings:* Limits on the number of SQL history items or file sizes.

### **2.2 Command Line Interface (CLI)**

To support scripting and power-user workflows, the application must accept specific command-line arguments that override default behaviors.

| Argument | Description | Requirement Source |
| :---- | :---- | :---- |
| \-h \<dir\> / \--home=\<dir\> | Overrides the application data directory (templates, default configs). | 4 |
| \-uh \<dir\> / \--user-home=\<dir\> | Overrides the user configuration directory (where fr\_databases.conf is read/written). | 4 |
| \-d \<db\_alias\> | Automatically attempts to connect to the database registered with the given alias upon launch. | Implied by 4 |
| \-q \<sql\_file\> | Launches an SQL Editor and loads the specified file. | Implied |

### **2.3 Deployment Structure**

The installation footprint must be minimal. The directory structure required for the application to function includes:

* /bin: The executable binary.  
* /html-templates: A directory containing .html files used for rendering object property pages. This must be user-accessible to allow for theme customization.4  
* /conf-defs: Default configuration templates for first-run initialization.  
* /docs: Offline documentation files.

## ---

**3\. The Main Window and Navigation Hierarchy**

The Main Window serves as the navigation hub. It presents a hierarchical view of the world, starting from the machine level down to the individual column or trigger level.

### **3.1 The Tree View Structure**

The application must utilize a standard Tree Control widget to render the hierarchy. The structure of this tree is strict and must follow the logical containment of the database engine.2

**Hierarchy Definition:**

1. **Root Node:** "Registered Servers" or "Home".  
2. **Server Node:** Represents a host address (e.g., localhost or 192.168.1.50).  
3. **Database Node:** Represents a specific connection context.  
   * *Visual State:* The icon for this node must change based on connection status (e.g., a red 'X' or greyed out when disconnected, a bright/colored icon when connected).  
4. **Object Containers:** Upon connection, the Database Node expands to show folder-like containers for object types:  
   * *Tables* (separating System Tables if configured).3  
   * *Views*  
   * *Procedures* (Stored Procedures)  
   * *Triggers* (Database-level triggers)  
   * *Functions* (UDFs or Stored Functions)  
   * *Generators/Sequences*  
   * *Domains/Types*  
   * *Roles*  
   * *Exceptions*  
5. **Object Nodes:** Individual entities (e.g., Table EMPLOYEES).  
6. **Sub-Entity Nodes:**  
   * Under a **Table**: Columns, Triggers, Indices, Constraints (PK/FK).  
   * Under a **Procedure**: Parameters, Source Code (optional).

**Interaction Logic:**

* **Lazy Loading:** The application must not fetch metadata for objects until the parent folder node is expanded. This ensures rapid connection times even for databases with thousands of objects.3  
* **Double-Click Action:** Configurable by the user. Default behavior should be to open the **Property Page** for the object. Alternatives include "Browse Data" or "Edit Object".3

### **3.2 Context Menu Specifications**

The depth of functionality in FlameRobin is primarily exposed via context menus on these tree nodes. The recreation must support the following specific actions per node type.

#### **3.2.1 Database Context Actions**

* **Connect/Disconnect:** Explicit control over the session.  
* **Reconnect:** A convenience function to drop and re-establish the connection (useful for resetting session variables).  
* **Registration Info:** Edit the alias, file path, or authentication credentials.  
* **Run Query:** Opens a blank SQL Editor connected to this database.  
* **Create New Object:** A submenu allowing the creation of any supported object type (Table, View, etc.). This opens an editor with a template CREATE statement.6  
* **Backup/Restore:** Launches the administrative dialogs.7  
* **Extract DDL:** Generates a complete SQL script to recreate the entire database schema.5  
* **Advanced:**  
  * *Show Connected Users:* Queries the monitoring tables to show active attachments.  
  * *Drop Database:* Deletes the database file (requires strict confirmation "Are you sure?").8

#### **3.2.2 Table Context Actions**

* **Browse Data:** Executes SELECT \* FROM table and opens the Data Grid.9  
* **Properties:** Opens the HTML property inspector.  
* **Insert/Update/Delete Templates:** Generates a script with placeholders for these operations based on the table's columns.  
* **Create Trigger:** Opens a wizard or template to create a trigger specifically bound to this table.6  
* **Drop:** Deletes the table.

#### **3.2.3 View Context Actions**

* **Rebuild View:** A critical maintenance feature. It extracts the view's definition, drops the view, and immediately recreates it. This is used to resynchronize metadata when underlying tables change.3  
* **Select From:** Generates a SELECT statement for the view.5

#### **3.2.4 Procedure Context Actions**

* **Execute:** Opens a parameter input dialog to run the procedure.6  
* **Select From:** (For selectable procedures) Generates a SELECT \* FROM procedure(...) statement.5  
* **Alter:** Opens the source code in the editor for modification.10

## ---

**4\. The SQL Executive (Editor)**

The SQL Editor is the primary workspace for the user. It is not merely a text box but a complex environment for code generation, execution, and analysis.

### **4.1 Syntax Highlighting and Visuals**

The editor must utilize a specialized control (like Scintilla) to provide rich text features.

* **Tokenization:** The parser must distinguish between keywords, literals (strings/numbers), comments, and known database object names.2  
* **Configuration:** Users must be able to customize the colors and font styles for each token type.  
* **Visual Guides:** Support for line numbering, code folding (collapsing blocks), and edge markers (e.g., at 80 characters).10

### **4.2 Auto-Completion and IntelliSense**

A robust auto-completion system is required to boost productivity.

* **Invocation:** Triggered automatically after a delimiter (like .) or manually via Ctrl+Space.12  
* **Context Awareness:** The autocomplete engine must be aware of the aliases defined in the current query. If FROM EMPLOYEES e is typed, typing e. should suggest columns from the EMPLOYEES table.13  
* **Dialect Support:** It must suggest internal functions (e.g., UPPER, CAST, COALESCE) relevant to the target database engine.13  
* **Call-Tips:** When typing a function or procedure name, a tooltip must appear showing the signature (parameter list and types). This helps users who don't memorize argument orders.2

### **4.3 The "Statement Splitter" and Execution Logic**

A distinct feature of FlameRobin is its ability to handle scripts containing multiple statements and delimiter changes (e.g., SET TERM in Firebird, DELIMITER in MySQL).

**Requirement:** The execution engine must not send the entire script to the database at once if the API doesn't support it. Instead, it must:

1. Parse the script linearly.  
2. Detect delimiter change commands.  
3. Split the text into individual command blocks based on the current delimiter.  
4. Execute each block sequentially.  
5. Stop immediately if an error occurs in any block, highlighting the specific line that failed.8

### **4.4 Transaction Control**

Unlike many IDEs that default to auto-commit, this tool must expose explicit transaction control.

* **Toolbar Controls:** "Commit" and "Rollback" buttons must be always visible.  
* **State Indication:** The editor needs to indicate if a transaction is currently "dirty" (has uncommitted changes).  
* **Isolation Configuration:** Users should be able to select the isolation level (e.g., Read Committed, Repeatable Read) for the current session.3  
* **Auto-Commit DDL Option:** A setting to automatically commit DDL statements (structure changes) while keeping DML (data changes) transactional.6

### **4.5 Execution Plan Analysis**

To aid in query optimization, the editor must support "Preparing" a query without executing it.

* **Plan Display:** The application must retrieve the execution plan from the engine.  
* **Formats:**  
  * *Text Plan:* The raw string returned by the engine (e.g., PLAN (A NATURAL)).14  
  * *Graphical Plan:* A tree visualization of the plan operations (e.g., Index Scan vs. Table Scan), potentially using color coding to highlight expensive operations.15  
* **Statistics:** Upon actual execution, the log pane must report detailed metrics: elapsed time, CPU time, page reads/writes, and indexed vs. non-indexed fetch counts.5

### **4.6 History Management**

* **Persistence:** Every executed statement is logged to a history file.  
* **Granularity:** The history should be persistent across sessions, stored on disk.  
* **Searchability:** The user must be able to search this history and reload past queries into the editor.2

## ---

**5\. Data Grid and Result Manipulation**

When a query returns a result set (cursor), the application displays it in a Data Grid. This grid is designed for both viewing and active manipulation.

### **5.1 Fetching Strategy and Pagination**

To handle large datasets efficiently without exhausting memory:

* **Fetch-on-Demand:** The grid initially fetches only a buffer of rows (e.g., 100). As the user scrolls to the bottom, the next batch is fetched automatically.16  
* **Fetch All:** A "Fetch All" command allows the user to force the retrieval of the entire dataset.  
* **Cancellation:** The user must be able to interrupt a "Fetch All" operation if it takes too long, leaving the grid with partial data.3

### **5.2 In-Place Editing**

The grid must function as a spreadsheet for the database.

* **Read-Only vs. Read-Write:** The application must detect if the result set is updatable (typically requires a single table source and inclusion of the Primary Key).  
* **Visual Feedback:** Edited cells should be highlighted (e.g., different background color). Deleted rows should be marked (e.g., strikethrough) rather than disappearing immediately.5  
* **SQL Generation:** When the user clicks "Save" or "Commit":  
  * Updates generate UPDATE table SET col=val WHERE pk=id.  
  * Inserts generate INSERT INTO table....  
  * Deletes generate DELETE FROM table WHERE pk=id.  
* **Logging:** These generated statements should be optionally logged to the history, allowing the user to audit the actual SQL used to perform the grid edits.5

### **5.3 BLOB Handling**

Binary Large Objects (BLOBs) require special handling since they cannot be rendered in a text cell.

* **Cell Representation:** Display a marker (e.g., \`\` or (MEMO)).  
* **Content Types:** The grid should attempt to detect the content type. If it is text, it may display the first few characters.  
* **BLOB Editor:** Double-clicking a BLOB cell opens a dedicated editor window with tabs:  
  * *Text View:* For viewing logs, XML, or JSON stored in BLOBs. Must support charset selection.  
  * *Hex View:* For inspecting raw binary data.  
  * *Image View:* Renders the data if it is a valid image format (PNG, JPG, BMP).  
  * *I/O:* Buttons to "Load from File" (upload a file into the BLOB) or "Save to File" (download the BLOB).3

## ---

**6\. Object Property Introspection (The HTML Engine)**

A distinctive feature of the reference application is its use of HTML templates to display object metadata. This allows for rich, hyperlinked, and customizable property pages.

### **6.1 Template Rendering Architecture**

Instead of hard-coded dialogs, the application uses an embedded HTML viewer widget.

* **Templates:** The html-templates directory contains files like TABLE.html, VIEW.html, DATABASE.html.  
* **Substitution:** The application queries the database metadata and injects it into the template.  
  * Example: A placeholder {%column\_list%} is replaced by an HTML \<table\> generated from the column metadata.  
* **Customization:** Advanced users can modify these HTML files to change the layout, fonts, or information density of the property pages without recompiling the program.2

### **6.2 Hyperlinking and Navigation**

The HTML engine must support a custom URI scheme to allow navigation within the database structure.

* **Scheme:** fr:// (or app://).  
* **Behavior:** A link \<a href="fr://table?name=DEPARTMENTS"\>DEPARTMENTS\</a\> in the Foreign Key list of the EMPLOYEES table should, when clicked, navigate the main tree to the DEPARTMENTS node and open its property page. This creates a "web-like" browsing experience for the database schema.2

### **6.3 Detailed Property Page Requirements**

#### **6.3.1 Table Properties**

The rendered page must display:

* **Columns:** Name, native datatype, size, nullable status, default values, and comments/descriptions.  
* **Constraints:** A list of Primary Keys, Foreign Keys (linking to target tables), Unique Constraints, and Check Constraints.2  
* **Indices:** List of indices with their status (Active/Inactive) and statistics (Selectivity).  
* **DDL Tab:** A tab or section showing the full CREATE TABLE script required to reproduce this object.2  
* **Triggers:** A list of triggers associated with the table, with the ability to expand/view their source code.3

#### **6.3.2 View Properties**

* **Definition:** The formatted SQL source code of the view.  
* **Dependencies:** A visual or listed representation of the dependency graph:  
  * *Uses:* Which tables/views this view reads from.  
  * *Used By:* Which other views/procedures reference this view.  
  * *Reasoning:* This allows the user to understand the impact of dropping or altering the view.2

## ---

**7\. Administrative and Maintenance Tools**

Beyond querying, the tool acts as a control panel for the database server.

### **7.1 Backup and Restore Subsystem**

The application must provide a graphical interface for the engine's backup tools (e.g., gbak for Firebird, pg\_dump for Postgres).

**Backup Dialog:**

* **Target Selection:** Output file path.  
* **Options Checkboxes:**  
  * *Ignore Checksums / Limbo Transactions:* For rescuing corrupted databases.  
  * *Garbage Collection:* Toggle garbage collection during backup.  
  * *Transportable Format:* Ensure cross-platform compatibility.  
* **Output Streaming:** The tool must capture the stdout/stderr of the backup process and display it in a scrolling log window within the dialog. This provides real-time feedback (e.g., "Writing table X...").18  
* **Threading:** The backup process must run in a separate thread so the main UI does not freeze.2

**Restore Dialog:**

* **Mode Selection:**  
  * *Replace Existing:* Overwrites the current database file.  
  * *Create New:* Restores to a new file path.19  
* **Post-Restore Actions:** Options to automatically activate indices or deactivate validity constraints (for partial restores).

### **7.2 User and Security Management**

**User Manager:**

* A list view of all users defined in the server's security database.  
* Forms to Add User, Edit User (change password/name), and Delete User.2  
* *Note:* This interacts with the server's global security context, not just the local database.

**Grant Manager:**

* A matrix-style interface for managing permissions (ACLs).  
* **Rows:** Users and Roles.  
* **Columns:** Privileges (SELECT, INSERT, UPDATE, DELETE, EXECUTE, REFERENCES).  
* **Logic:** Toggles in the grid queue up GRANT or REVOKE statements which are applied in a batch when "Save" is clicked.2

### **7.3 Event Monitoring**

For engines supporting asynchronous notification channels (events):

* **Subscription Interface:** A dialog allowing the user to input names of events to listen for.  
* **Monitor Window:** A log window that stays open. When the server posts an event, this window displays the event name, timestamp, and payload count.  
* **Usage:** This is critical for debugging applications that use database triggers to signal external software.10

## ---

**8\. DDL Generation and Schema Reverse Engineering**

A core capability of the tool is to reverse-engineer the database schema into SQL scripts.

### **8.1 The "Extract DDL" Engine**

The application must include a generator capable of producing the CREATE script for any object, or the entire database.

**Requirement:** The generator must respect dependency order.

* *Incorrect:* Creating a Table before the Domain (Type) it uses exists.  
* *Correct:* CREATE DOMAIN \-\> CREATE TABLE \-\> CREATE INDEX \-\> CREATE TRIGGER.  
* *Mechanism:* The generator builds a dependency graph of all objects and performs a topological sort to determine the output order.2

### **8.2 The "Rebuild" Macro**

Often, database objects (especially Views and Procedures) become invalid if their dependencies change (e.g., a column is renamed). The tool must offer a "Rebuild" macro.

**Workflow:**

1. **Extract:** Get the current DDL of the object.  
2. **Drop:** Execute DROP VIEW X.  
3. **Create:** Execute the extracted DDL.  
4. **Transaction:** Wrap the entire operation in a single transaction. If the "Create" step fails (due to the underlying error), the transaction rolls back, restoring the original (albeit invalid) object, ensuring the database is not left in a partial state.8

## ---

**9\. Drag-and-Drop Query Construction**

To assist in rapid query prototyping, the application supports drag-and-drop interactions between the Tree View and the SQL Editor.

**Behaviors:**

* **Drag Table \-\> Editor:** Generates SELECT \* FROM Table.  
* **Drag Column \-\> Editor:** Inserts the column name.  
* **Advanced Smart Join:**  
  * *Scenario:* User drags Table A into the editor. Then drags Table B.  
  * *Logic:* The application checks for Foreign Keys between A and B.  
  * *Result:* It generates a JOIN clause automatically: SELECT... FROM Table A JOIN Table B ON A.id \= B.a\_id.2  
  * *Ambiguity:* If multiple FK paths exist, a popup menu asks the user which relationship to use.

## ---

**10\. Conclusion and Implementation Roadmap**

Recreating FlameRobin for a new database engine is a significant undertaking that requires a disciplined approach to UI architecture and database metadata management.

**Implementation Steps:**

1. **Core Framework:** Establish the C++ application skeleton using wxWidgets (or equivalent native toolkit) and the SDI window manager.  
2. **Connectivity Layer:** Implement the connection to the target database API and the basic DBH classes.  
3. **Metadata Layer:** Write the queries to extract schema information (Tables, Columns, etc.) from the target engine's system catalogs.  
4. **Observer System:** Implement the notification signal system to link the DBH to the UI.  
5. **Main UI:** Build the Tree View and the HTML Property Viewer.  
6. **Editors:** Implement the SQL Editor with highlighting and the Data Grid with fetching logic.  
7. **Admin Tools:** specific implementations for Backup, Restore, and User management.

By following this specification, the resulting tool will not only function as a query runner but as a comprehensive, living interface to the database, providing the same high-efficiency workflow that FlameRobin users rely on. The focus on native execution, low memory overhead, and strict state synchronization via the Observer pattern are the non-negotiable pillars of this design.

| Feature Area | Priority | Key Technical Challenge |
| :---- | :---- | :---- |
| **Tree View** | High | Lazy loading nodes for performance. |
| **SQL Editor** | High | Statement splitting and auto-completion cache. |
| **Data Grid** | High | Virtual fetching and BLOB handling. |
| **Property Pages** | Medium | Implementing the HTML template engine. |
| **Backup/Restore** | Medium | Threading and output stream parsing. |
| **Event Monitor** | Low | Engine-specific implementation of async events. |

This specification serves as the master blueprint for the development of the "Next-Gen FlameRobin" for the target database platform.

#### **Works cited**

1. FlameRobin is a database administration tool for Firebird RDBMS. Our goal is to build a tool that is: lightweight (small footprint, fast execution) cross-platform (Linux, Windows, Mac OS, FreeBSD) dependent only on other Open Source software \- GitHub, accessed December 7, 2025, [https://github.com/mariuz/flamerobin](https://github.com/mariuz/flamerobin)  
2. Administration tool for Firebird \- FlameRobin, accessed December 7, 2025, [http://www.flamerobin.org/flamerobin\_paper.pdf](http://www.flamerobin.org/flamerobin_paper.pdf)  
3. flamerobin/docs/fr\_whatsnew.html at master \- GitHub, accessed December 7, 2025, [https://github.com/mariuz/flamerobin/blob/master/docs/fr\_whatsnew.html](https://github.com/mariuz/flamerobin/blob/master/docs/fr_whatsnew.html)  
4. flamerobin — management and data manipulation tool for the Firebird DBMS \- Ubuntu Manpage, accessed December 7, 2025, [https://manpages.ubuntu.com/manpages/jammy/man1/flamerobin.1.html](https://manpages.ubuntu.com/manpages/jammy/man1/flamerobin.1.html)  
5. FlameRobin 0.8.3, accessed December 7, 2025, [http://www.flamerobin.org/releases/0.8.3.html](http://www.flamerobin.org/releases/0.8.3.html)  
6. FlameRobin 0.4.0, accessed December 7, 2025, [http://www.flamerobin.org/releases/0.4.0.html](http://www.flamerobin.org/releases/0.4.0.html)  
7. Firebird database Backup & Restore \- es2000, accessed December 7, 2025, [https://manual.es2000.de/install/en-us/firebird/backup\_restore\_firebird.htm?TocPath=Installations%7CFirebird%20server%7C\_\_\_\_\_1](https://manual.es2000.de/install/en-us/firebird/backup_restore_firebird.htm?TocPath=Installations%7CFirebird+server%7C_____1)  
8. FlameRobin / News \- SourceForge, accessed December 7, 2025, [https://sourceforge.net/p/flamerobin/news/](https://sourceforge.net/p/flamerobin/news/)  
9. FlameRobin \- YouTube, accessed December 7, 2025, [https://www.youtube.com/watch?v=yYC\_EGA8Z6Q](https://www.youtube.com/watch?v=yYC_EGA8Z6Q)  
10. FlameRobin 0.5.0, accessed December 7, 2025, [http://flamerobin.org/releases/0.5.0.html](http://flamerobin.org/releases/0.5.0.html)  
11. SQL code editor with syntax highlighing, auto-formatting and code folding \- Stack Overflow, accessed December 7, 2025, [https://stackoverflow.com/questions/2950477/sql-code-editor-with-syntax-highlighing-auto-formatting-and-code-folding](https://stackoverflow.com/questions/2950477/sql-code-editor-with-syntax-highlighing-auto-formatting-and-code-folding)  
12. Online Documentation for SQL Query for InterBase/FireBird | SQLManager, accessed December 7, 2025, [https://www.sqlmanager.net/products/ibfb/query/documentation/hs3311.html](https://www.sqlmanager.net/products/ibfb/query/documentation/hs3311.html)  
13. Linux SQL Editor: Anything that does proper auto-completion and auto-lookup and is NOT Datagrip? \- Reddit, accessed December 7, 2025, [https://www.reddit.com/r/SQL/comments/eyi33g/linux\_sql\_editor\_anything\_that\_does\_proper/](https://www.reddit.com/r/SQL/comments/eyi33g/linux_sql_editor_anything_that_does_proper/)  
14. Delphi FireDAC Firebird programatically get execution plan \- Stack Overflow, accessed December 7, 2025, [https://stackoverflow.com/questions/59553110/delphi-firedac-firebird-programatically-get-execution-plan](https://stackoverflow.com/questions/59553110/delphi-firedac-firebird-programatically-get-execution-plan)  
15. Query execution plan | DataGrip Documentation \- JetBrains, accessed December 7, 2025, [https://www.jetbrains.com/help/datagrip/query-execution-plan.html](https://www.jetbrains.com/help/datagrip/query-execution-plan.html)  
16. ResultSet FetchSize \-- Pagination of long lasting queries \- Stack Overflow, accessed December 7, 2025, [https://stackoverflow.com/questions/35510940/resultset-fetchsize-pagination-of-long-lasting-queries](https://stackoverflow.com/questions/35510940/resultset-fetchsize-pagination-of-long-lasting-queries)  
17. BLOB Editor \- Firebird Maestro online Help, accessed December 7, 2025, [https://www.sqlmaestro.com/products/firebird/maestro/help/05\_03\_00\_blob\_editor/](https://www.sqlmaestro.com/products/firebird/maestro/help/05_03_00_blob_editor/)  
18. FlameRobin / Bugs / \#82 Restore is failing with a strange error \- SourceForge, accessed December 7, 2025, [https://sourceforge.net/p/flamerobin/bugs/82/](https://sourceforge.net/p/flamerobin/bugs/82/)  
19. Firebird test environment \- es2000, accessed December 7, 2025, [https://manual.es2000.de/install/en-us/esoffice/faq/testumgebung\_firebird.htm](https://manual.es2000.de/install/en-us/esoffice/faq/testumgebung_firebird.htm)