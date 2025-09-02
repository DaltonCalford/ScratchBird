# T-SQL (Microsoft SQL Server) BNF/EBNF Grammar

## Overview
This document provides a comprehensive BNF/EBNF grammar specification for T-SQL (Transact-SQL), Microsoft SQL Server's extension of SQL. T-SQL adds procedural programming, local variables, various support functions for string processing, date processing, mathematics, etc., and extends the standard SQL DDL and DML statements.

## Notation
- `::=` defines a production rule
- `|` indicates alternatives
- `[ ]` indicates optional elements
- `{ }` indicates zero or more repetitions
- `( )` groups elements
- `< >` denotes non-terminals
- Terminal symbols are in UPPERCASE or quoted
- `/* Version */` indicates version-specific features

## Core Grammar

### 1. Data Types

```ebnf
<tsql_data_type> ::=
    <exact_numeric_type>
  | <approximate_numeric_type>
  | <date_time_type>
  | <character_string_type>
  | <unicode_string_type>
  | <binary_string_type>
  | <other_data_type>

<exact_numeric_type> ::=
    BIT
  | TINYINT
  | SMALLINT
  | INT | INTEGER
  | BIGINT
  | DECIMAL [ ( <precision> [ , <scale> ] ) ]
  | DEC [ ( <precision> [ , <scale> ] ) ]
  | NUMERIC [ ( <precision> [ , <scale> ] ) ]
  | SMALLMONEY
  | MONEY

<approximate_numeric_type> ::=
    FLOAT [ ( <mantissa> ) ]
  | REAL

<date_time_type> ::=
    DATE  /* SQL Server 2008+ */
  | TIME [ ( <fractional_seconds> ) ]  /* SQL Server 2008+ */
  | DATETIME
  | DATETIME2 [ ( <fractional_seconds> ) ]  /* SQL Server 2008+ */
  | SMALLDATETIME
  | DATETIMEOFFSET [ ( <fractional_seconds> ) ]  /* SQL Server 2008+ */

<character_string_type> ::=
    CHAR [ ( <length> ) ] [ COLLATE <collation_name> ]
  | CHARACTER [ ( <length> ) ] [ COLLATE <collation_name> ]
  | VARCHAR [ ( <length> | MAX ) ] [ COLLATE <collation_name> ]
  | CHARACTER VARYING [ ( <length> | MAX ) ] [ COLLATE <collation_name> ]
  | TEXT [ COLLATE <collation_name> ]  /* Deprecated */

<unicode_string_type> ::=
    NCHAR [ ( <length> ) ] [ COLLATE <collation_name> ]
  | NVARCHAR [ ( <length> | MAX ) ] [ COLLATE <collation_name> ]
  | NTEXT [ COLLATE <collation_name> ]  /* Deprecated */

<binary_string_type> ::=
    BINARY [ ( <length> ) ]
  | VARBINARY [ ( <length> | MAX ) ]
  | IMAGE  /* Deprecated */

<other_data_type> ::=
    CURSOR  /* Variables only */
  | HIERARCHYID  /* SQL Server 2008+ */
  | SQL_VARIANT
  | TABLE  /* Variables and parameters */
  | TIMESTAMP | ROWVERSION
  | UNIQUEIDENTIFIER
  | XML [ ( [ <xml_schema_collection> ] ) ]
  | GEOMETRY  /* SQL Server 2008+ */
  | GEOGRAPHY  /* SQL Server 2008+ */
  | JSON  /* SQL Server 2016+ for JSON operations */
```

### 2. DDL Statements

#### CREATE TABLE

```ebnf
<create_table_statement> ::=
    CREATE TABLE [ <database_name> . [ <schema_name> ] . | <schema_name> . ] <table_name>
    { ( <table_element> { , <table_element> } )
      [ ON { <partition_scheme> ( <partition_column> ) | <filegroup> | DEFAULT } ]
      [ TEXTIMAGE_ON { <filegroup> | DEFAULT } ]
      [ FILESTREAM_ON { <partition_scheme> | <filegroup> | DEFAULT } ]
      [ WITH ( <table_option> { , <table_option> } ) ]
    | AS <select_statement> [ WITH ( <table_option> { , <table_option> } ) ] }  /* SQL Server 2014+ */

<table_element> ::=
    <column_definition>
  | <computed_column_definition>
  | <column_set_definition>  /* SQL Server 2008+ */
  | <table_constraint>
  | <index_definition>  /* SQL Server 2014+ */
  | <period_definition>  /* SQL Server 2016+ */

<column_definition> ::=
    <column_name> <data_type>
    [ FILESTREAM ]  /* For varbinary(max) */
    [ COLLATE <collation_name> ]
    [ SPARSE ]  /* SQL Server 2008+ */
    [ MASKED WITH ( FUNCTION = '<mask_function>' ) ]  /* SQL Server 2016+ */
    [ [ CONSTRAINT <constraint_name> ] DEFAULT <default_expression> ]
    [ IDENTITY [ ( <seed> , <increment> ) ] [ NOT FOR REPLICATION ] ]
    [ GENERATED ALWAYS AS { ROW START | ROW END } [ HIDDEN ] ]  /* SQL Server 2016+ */
    [ NULL | NOT NULL ]
    [ ROWGUIDCOL ]
    [ ENCRYPTED WITH ( <encryption_options> ) ]  /* SQL Server 2016+ */
    [ <column_constraint> { <column_constraint> } ]
    [ <column_index> ]  /* SQL Server 2014+ */

<computed_column_definition> ::=
    <column_name> AS <expression>
    [ PERSISTED [ NOT NULL ] ]
    [ [ CONSTRAINT <constraint_name> ] <column_constraint> { <column_constraint> } ]

<column_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    { [ NULL | NOT NULL ]
    | [ { PRIMARY KEY | UNIQUE } ] [ CLUSTERED | NONCLUSTERED ]
        [ WITH FILLFACTOR = <fillfactor> 
        | WITH ( <index_option> { , <index_option> } ) ]
        [ ON { <partition_scheme> ( <partition_column> ) | <filegroup> | DEFAULT } ]
    | [ FOREIGN KEY ] REFERENCES <referenced_table> [ ( <referenced_column> ) ]
        [ ON DELETE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
        [ ON UPDATE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
        [ NOT FOR REPLICATION ]
    | CHECK [ NOT FOR REPLICATION ] ( <logical_expression> ) }

<table_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    { { PRIMARY KEY | UNIQUE } [ CLUSTERED | NONCLUSTERED ] ( <column_list> )
        [ WITH FILLFACTOR = <fillfactor> 
        | WITH ( <index_option> { , <index_option> } ) ]
        [ ON { <partition_scheme> ( <partition_column> ) | <filegroup> | DEFAULT } ]
    | FOREIGN KEY ( <column_list> ) REFERENCES <referenced_table> [ ( <column_list> ) ]
        [ ON DELETE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
        [ ON UPDATE { NO ACTION | CASCADE | SET NULL | SET DEFAULT } ]
        [ NOT FOR REPLICATION ]
    | CHECK [ NOT FOR REPLICATION ] ( <logical_expression> ) }

<table_option> ::=
    DATA_COMPRESSION = { NONE | ROW | PAGE | COLUMNSTORE | COLUMNSTORE_ARCHIVE }
        [ ON PARTITIONS ( <partition_list> ) ]
  | XML_COMPRESSION = { ON | OFF } [ ON PARTITIONS ( <partition_list> ) ]  /* SQL Server 2022+ */
  | FILETABLE_DIRECTORY = '<directory_name>'  /* SQL Server 2012+ */
  | FILETABLE_COLLATE_FILENAME = { <collation_name> | DATABASE_DEFAULT }
  | FILETABLE_PRIMARY_KEY_CONSTRAINT_NAME = <constraint_name>
  | FILETABLE_STREAMID_UNIQUE_CONSTRAINT_NAME = <constraint_name>
  | FILETABLE_FULLPATH_UNIQUE_CONSTRAINT_NAME = <constraint_name>
  | SYSTEM_VERSIONING = ON 
        [ ( HISTORY_TABLE = <schema_name>.<history_table_name> 
            [ , DATA_CONSISTENCY_CHECK = { ON | OFF } ] ) ]  /* SQL Server 2016+ */
  | REMOTE_DATA_ARCHIVE = { ON [ ( <remote_data_archive_options> ) ] | OFF }  /* SQL Server 2016+ */
  | MEMORY_OPTIMIZED = ON  /* SQL Server 2014+ */
  | DURABILITY = { SCHEMA_ONLY | SCHEMA_AND_DATA }  /* SQL Server 2014+ */
  | LEDGER = ON [ ( <ledger_options> ) ]  /* SQL Server 2022+ */
  | LOCK_ESCALATION = { AUTO | TABLE | DISABLE }
  | TRACK_COLUMNS_UPDATED = { ON | OFF }
```

#### CREATE INDEX

```ebnf
<create_index_statement> ::=
    CREATE [ UNIQUE ] [ CLUSTERED | NONCLUSTERED ] INDEX <index_name>
    ON [ <database_name> . [ <schema_name> ] . | <schema_name> . ] <object>
    ( <column> [ ASC | DESC ] { , <column> [ ASC | DESC ] } )
    [ INCLUDE ( <column_name> { , <column_name> } ) ]
    [ WHERE <filter_predicate> ]  /* Filtered index */
    [ WITH ( <index_option> { , <index_option> } ) ]
    [ ON { <partition_scheme> ( <column_name> ) | <filegroup_name> | DEFAULT } ]
    [ FILESTREAM_ON { <filestream_filegroup> | <partition_scheme> | NULL } ]

<columnstore_index> ::=
    CREATE [ CLUSTERED | NONCLUSTERED ] COLUMNSTORE INDEX <index_name>
    ON [ <database_name> . [ <schema_name> ] . | <schema_name> . ] <table_name>
    [ ( <column_name> { , <column_name> } ) ]  /* NONCLUSTERED only */
    [ WHERE <filter_predicate> ]  /* SQL Server 2016+ */
    [ WITH ( <columnstore_index_option> { , <columnstore_index_option> } ) ]
    [ ON { <partition_scheme> ( <column_name> ) | <filegroup_name> | DEFAULT } ]

<index_option> ::=
    PAD_INDEX = { ON | OFF }
  | FILLFACTOR = <fillfactor>
  | SORT_IN_TEMPDB = { ON | OFF }
  | IGNORE_DUP_KEY = { ON | OFF }
  | STATISTICS_NORECOMPUTE = { ON | OFF }
  | STATISTICS_INCREMENTAL = { ON | OFF }  /* SQL Server 2014+ */
  | DROP_EXISTING = { ON | OFF }
  | ONLINE = { ON [ ( <low_priority_lock_wait> ) ] | OFF }
  | RESUMABLE = { ON | OFF }  /* SQL Server 2017+ */
  | MAX_DURATION = <time> [ MINUTES ]  /* SQL Server 2017+ */
  | ALLOW_ROW_LOCKS = { ON | OFF }
  | ALLOW_PAGE_LOCKS = { ON | OFF }
  | OPTIMIZE_FOR_SEQUENTIAL_KEY = { ON | OFF }  /* SQL Server 2019+ */
  | MAXDOP = <max_degree_of_parallelism>
  | DATA_COMPRESSION = { NONE | ROW | PAGE | COLUMNSTORE | COLUMNSTORE_ARCHIVE }
        [ ON PARTITIONS ( <partition_list> ) ]
  | XML_COMPRESSION = { ON | OFF } [ ON PARTITIONS ( <partition_list> ) ]  /* SQL Server 2022+ */
  | COMPRESSION_DELAY = <delay> [ MINUTES ]
  | BUCKET_COUNT = <bucket_count>  /* Memory-optimized tables only */
```

#### CREATE VIEW

```ebnf
<create_view_statement> ::=
    CREATE [ OR ALTER ] VIEW [ <schema_name> . ] <view_name> 
    [ ( <column_name> { , <column_name> } ) ]
    [ WITH { ENCRYPTION | SCHEMABINDING | VIEW_METADATA } { , ... } ]
    AS <select_statement>
    [ WITH CHECK OPTION ]

<indexed_view> ::=
    /* Create view WITH SCHEMABINDING, then create UNIQUE CLUSTERED INDEX */
```

#### CREATE SCHEMA

```ebnf
<create_schema_statement> ::=
    CREATE SCHEMA { <schema_name> | AUTHORIZATION <owner_name> 
                  | <schema_name> AUTHORIZATION <owner_name> }
    [ <schema_element> { <schema_element> } ]

<schema_element> ::=
    <create_table_statement>
  | <create_view_statement>
  | <grant_statement>
  | <revoke_statement>
  | <deny_statement>
```

### 3. DML Statements

#### INSERT

```ebnf
<insert_statement> ::=
    [ WITH <common_table_expression> { , <common_table_expression> } ]
    INSERT [ TOP ( <expression> ) [ PERCENT ] ] 
    [ INTO ] { <table_or_view> | <rowset_function> | <openquery> }
    { [ ( <column_list> ) ] 
      [ OUTPUT <output_clause> ]
      { VALUES ( { <expression> | DEFAULT | NULL } { , ... } ) { , ( ... ) }
      | <derived_table>
      | <execute_statement>
      | DEFAULT VALUES } }

<bulk_insert> ::=
    BULK INSERT [ <database_name> . [ <schema_name> ] . | <schema_name> . ] <table_name>
    FROM '<data_file>'
    [ WITH ( <bulk_insert_option> { , <bulk_insert_option> } ) ]
```

#### UPDATE

```ebnf
<update_statement> ::=
    [ WITH <common_table_expression> { , <common_table_expression> } ]
    UPDATE [ TOP ( <expression> ) [ PERCENT ] ] 
    { <table_or_view> | <rowset_function> | <openquery> }
    SET { <column_name> = { <expression> | DEFAULT | NULL }
        | <variable> = <expression>
        | <variable> = <column> = <expression>
        | <column_name> { .WRITE ( <expression> , @Offset , @Length ) }
        | @<variable> = <expression> } { , ... }
    [ OUTPUT <output_clause> ]
    [ FROM <table_source> { , <table_source> } ]
    [ WHERE { <search_condition> | CURRENT OF { { [ GLOBAL ] <cursor_name> } | <cursor_variable> } } ]
    [ OPTION ( <query_hint> { , <query_hint> } ) ]
```

#### DELETE

```ebnf
<delete_statement> ::=
    [ WITH <common_table_expression> { , <common_table_expression> } ]
    DELETE [ TOP ( <expression> ) [ PERCENT ] ]
    [ FROM ] { <table_or_view> | <rowset_function> | <openquery> }
    [ OUTPUT <output_clause> ]
    [ FROM <table_source> { , <table_source> } ]
    [ WHERE { <search_condition> | CURRENT OF { { [ GLOBAL ] <cursor_name> } | <cursor_variable> } } ]
    [ OPTION ( <query_hint> { , <query_hint> } ) ]

<truncate_table> ::=
    TRUNCATE TABLE [ <database_name> . [ <schema_name> ] . | <schema_name> . ] <table_name>
    [ WITH ( PARTITIONS ( <partition_list> ) ) ]  /* SQL Server 2016+ */
```

#### MERGE

```ebnf
<merge_statement> ::=
    [ WITH <common_table_expression> { , <common_table_expression> } ]
    MERGE [ TOP ( <expression> ) [ PERCENT ] ]
    [ INTO ] <target_table> [ [ AS ] <alias> ]
    USING <table_source> ON <merge_search_condition>
    [ <when_matched_clause> ]
    [ <when_not_matched_by_target_clause> ]
    [ <when_not_matched_by_source_clause> ]
    [ OUTPUT <output_clause> ]
    [ OPTION ( <query_hint> { , <query_hint> } ) ] ;

<when_matched_clause> ::=
    WHEN MATCHED [ AND <search_condition> ] THEN
    { UPDATE SET { <column_name> = <expression> } { , ... } | DELETE }

<when_not_matched_by_target_clause> ::=
    WHEN NOT MATCHED [ BY TARGET ] [ AND <search_condition> ] THEN
    INSERT [ ( <column_list> ) ] { VALUES ( <value_list> ) | DEFAULT VALUES }

<when_not_matched_by_source_clause> ::=
    WHEN NOT MATCHED BY SOURCE [ AND <search_condition> ] THEN
    { UPDATE SET { <column_name> = <expression> } { , ... } | DELETE }
```

#### SELECT

```ebnf
<select_statement> ::=
    [ WITH <common_table_expression> { , <common_table_expression> } ]
    <query_expression>
    [ ORDER BY <order_by_clause> ]
    [ FOR { BROWSE 
          | XML { RAW [ ( '<element_name>' ) ] | AUTO | EXPLICIT | PATH [ ( '<element_name>' ) ] }
              [ , { XMLDATA | XMLSCHEMA [ ( '<schema>' ) ] } ]
              [ , ELEMENTS [ { XSINIL | ABSENT } ] ]
              [ , ROOT [ ( '<root_name>' ) ] ]
              [ , TYPE ]
              [ , BINARY BASE64 ]
          | JSON { AUTO | PATH } [ , ROOT [ ( '<root_name>' ) ] ] 
              [ , INCLUDE_NULL_VALUES ] [ , WITHOUT_ARRAY_WRAPPER ]  /* SQL Server 2016+ */ } ]
    [ OPTION ( <query_hint> { , <query_hint> } ) ]

<query_expression> ::=
    { <query_specification> | ( <query_expression> ) }
    [ { UNION [ ALL ] | EXCEPT | INTERSECT } <query_specification> | ( <query_expression> ) ]

<query_specification> ::=
    SELECT [ ALL | DISTINCT ] [ TOP ( <expression> ) [ PERCENT ] [ WITH TIES ] ]
    <select_list>
    [ INTO <new_table> ]
    [ FROM <table_source> { , <table_source> } ]
    [ WHERE <search_condition> ]
    [ GROUP BY [ ALL ] <group_by_item> { , <group_by_item> } [ WITH { CUBE | ROLLUP } ] ]
    [ HAVING <search_condition> ]

<select_list> ::=
    { * 
    | { <table_name> | <table_alias> } . * 
    | { <expression> [ [ AS ] <column_alias> ] 
      | <column_alias> = <expression> } } { , ... }

<table_source> ::=
    { <table_or_view_name> [ [ AS ] <alias> ] [ WITH ( <table_hint> { , <table_hint> } ) ]
    | <rowset_function> [ [ AS ] <alias> ] [ ( <column_alias_list> ) ]
    | <openquery>
    | <openrowset>
    | <opendatasource>
    | <openxml>
    | <derived_table> [ [ AS ] <alias> ] [ ( <column_alias_list> ) ]
    | <joined_table>
    | <pivoted_table>
    | <unpivoted_table>
    | <table_valued_function>
    | CHANGETABLE ( { CHANGES | VERSION } <table_name> , ... )  /* SQL Server 2008+ */
    | <graph_table> }  /* SQL Server 2017+ */

<joined_table> ::=
    <table_source> <join_type> <table_source> ON <search_condition>
  | <table_source> CROSS JOIN <table_source>
  | <table_source> { CROSS | OUTER } APPLY <table_source>

<join_type> ::=
    [ INNER ] JOIN
  | { LEFT | RIGHT | FULL } [ OUTER ] JOIN

<pivoted_table> ::=
    <table_source> PIVOT ( <aggregate_function> ( <value_column> )
    FOR <pivot_column> IN ( <column_list> ) ) [ [ AS ] <alias> ]

<unpivoted_table> ::=
    <table_source> UNPIVOT ( <value_column> 
    FOR <pivot_column> IN ( <column_list> ) ) [ [ AS ] <alias> ]

<derived_table> ::=
    ( <select_statement> )

<common_table_expression> ::=
    <cte_name> [ ( <column_name> { , <column_name> } ) ]
    AS ( <cte_query_definition> )
```

### 4. Procedural Extensions

#### Stored Procedures

```ebnf
<create_procedure> ::=
    CREATE [ OR ALTER ] { PROC | PROCEDURE } [ <schema_name> . ] <procedure_name>
    [ ; <number> ]  /* Numbered procedures - deprecated */
    [ { @<parameter_name> [ <type_schema_name> . ] <data_type>
        [ VARYING ] [ = <default> ] [ OUT | OUTPUT ] [ READONLY ] } ] { , ... }
    [ WITH { ENCRYPTION | RECOMPILE | EXECUTE AS <execute_as> } { , ... } ]
    [ FOR REPLICATION ]
    AS { [ BEGIN ] <sql_statement> { ; <sql_statement> } [ END ] | EXTERNAL NAME <assembly_method> }

<execute_procedure> ::=
    [ { EXEC | EXECUTE } ] { [ @<return_status> = ] <procedure_name> [ ; <number> ] 
                           | @<procedure_name_var> }
    [ { @<parameter_name> = } { <value> | @<variable> [ OUTPUT ] | DEFAULT } ] { , ... }
    [ WITH { RECOMPILE | RESULT SETS { UNDEFINED | NONE | <result_sets_definition> } } ]
```

#### Functions

```ebnf
<create_function> ::=
    CREATE [ OR ALTER ] FUNCTION [ <schema_name> . ] <function_name>
    ( [ { @<parameter_name> [ AS ] [ <type_schema_name> . ] <parameter_data_type>
        [ = <default> ] [ READONLY ] } ] { , ... } )
    RETURNS { <return_data_type> | TABLE [ <table_type_definition> ] }
    [ WITH { ENCRYPTION | SCHEMABINDING | RETURNS NULL ON NULL INPUT 
           | CALLED ON NULL INPUT | EXECUTE AS <execute_as> 
           | INLINE = { ON | OFF } } { , ... } ]  /* INLINE: SQL Server 2019+ */
    [ AS ]
    { RETURN <scalar_expression>  /* Scalar function */
    | BEGIN <function_body> RETURN <scalar_expression> END  /* Multi-statement scalar */
    | RETURN [ ( ] <select_statement> [ ) ]  /* Inline table-valued */
    | BEGIN <function_body> RETURN END  /* Multi-statement table-valued */
    | EXTERNAL NAME <assembly_method> }  /* CLR function */

<table_type_definition> ::=
    ( { <column_definition> | <computed_column_definition> 
      | <table_constraint> | <index_definition> } { , ... } )
```

#### Triggers

```ebnf
<create_trigger> ::=
    CREATE [ OR ALTER ] TRIGGER [ <schema_name> . ] <trigger_name>
    ON { <table_name> | DATABASE | ALL SERVER }
    [ WITH { ENCRYPTION | EXECUTE AS <execute_as> } { , ... } ]
    { { FOR | AFTER | INSTEAD OF } { [ INSERT ] [ , ] [ UPDATE ] [ , ] [ DELETE ] }
      [ WITH APPEND ]  /* Deprecated */
      [ NOT FOR REPLICATION ]
      AS <sql_statement> { ; <sql_statement> } }
    | { FOR | AFTER } { <ddl_event> } { , ... }
      AS <sql_statement> { ; <sql_statement> }  /* DDL trigger */
    | { FOR | AFTER } LOGON
      AS <sql_statement> { ; <sql_statement> }  /* Logon trigger */

<enable_disable_trigger> ::=
    { ENABLE | DISABLE } TRIGGER { [ <schema_name> . ] <trigger_name> | ALL }
    ON { [ <schema_name> . ] <object_name> | DATABASE | ALL SERVER }
```

### 5. Control Flow

```ebnf
<control_flow_statement> ::=
    <begin_end_block>
  | <if_else_statement>
  | <while_statement>
  | <break_statement>
  | <continue_statement>
  | <goto_statement>
  | <return_statement>
  | <waitfor_statement>
  | <try_catch_block>
  | <throw_statement>

<begin_end_block> ::=
    BEGIN { <sql_statement> ; } END

<if_else_statement> ::=
    IF <boolean_expression> <sql_statement> [ ; ]
    [ ELSE <sql_statement> [ ; ] ]

<while_statement> ::=
    WHILE <boolean_expression> <sql_statement> [ ; ]
    [ BREAK ] [ CONTINUE ]

<break_statement> ::= BREAK

<continue_statement> ::= CONTINUE

<goto_statement> ::= GOTO <label>
<label_declaration> ::= <label> :

<return_statement> ::= RETURN [ <integer_expression> ]

<waitfor_statement> ::=
    WAITFOR { DELAY '<time_interval>' | TIME '<time>' 
            | [ ( ] <receive_statement> [ ) ] [ , TIMEOUT <timeout> ] }

<try_catch_block> ::=
    BEGIN TRY
        { <sql_statement> ; }
    END TRY
    BEGIN CATCH
        { <sql_statement> ; }
    END CATCH

<throw_statement> ::=
    THROW [ { <error_number> , '<message>' , <state> } ] ;  /* SQL Server 2012+ */

<raiserror_statement> ::=
    RAISERROR ( { <msg_id> | <msg_str> | @<local_variable> }
        { , <severity> , <state> }
        [ , <argument> { , <argument> } ] )
    [ WITH { LOG | NOWAIT | SETERROR } { , ... } ]
```

### 6. Variables and Parameters

```ebnf
<declare_statement> ::=
    DECLARE { @<local_variable> [ AS ] <data_type> [ = <value> ] } { , ... }
  | DECLARE @<local_variable> [ AS ] TABLE <table_type_definition>
  | DECLARE @<local_variable> [ AS ] [ <type_schema_name> . ] <user_defined_table_type>
  | DECLARE <cursor_name> [ INSENSITIVE ] [ SCROLL ] CURSOR 
    [ FOR { <select_statement> [ FOR { READ ONLY | UPDATE [ OF <column_list> ] } ] } ]
    [ LOCAL | GLOBAL ] [ FORWARD_ONLY | SCROLL ] 
    [ STATIC | KEYSET | DYNAMIC | FAST_FORWARD ]
    [ READ_ONLY | SCROLL_LOCKS | OPTIMISTIC ]
    [ TYPE_WARNING ]

<set_statement> ::=
    SET { @<local_variable> = <expression>
        | @<local_variable> = <column> = <expression>
        | @<local_variable> { += | -= | *= | /= | %= | &= | ^= | |= } <expression>
        | @<cursor_variable> = CURSOR [ FORWARD_ONLY | SCROLL ] 
            [ STATIC | KEYSET | DYNAMIC | FAST_FORWARD ]
            [ READ_ONLY | SCROLL_LOCKS | OPTIMISTIC ]
            [ TYPE_WARNING ]
            FOR <select_statement>
            [ FOR { READ ONLY | UPDATE [ OF <column_list> ] } ] }

<select_assignment> ::=
    SELECT { @<local_variable> = <expression> } { , ... }
    [ FROM <table_source> { , <table_source> } ]
    [ WHERE <search_condition> ]
    /* Other SELECT clauses */
```

### 7. Cursors

```ebnf
<cursor_operations> ::=
    <declare_cursor>
  | <open_cursor>
  | <fetch_cursor>
  | <close_cursor>
  | <deallocate_cursor>

<open_cursor> ::=
    OPEN { { [ GLOBAL ] <cursor_name> } | <cursor_variable> }

<fetch_cursor> ::=
    FETCH [ [ NEXT | PRIOR | FIRST | LAST 
           | ABSOLUTE { <n> | @<nvar> } 
           | RELATIVE { <n> | @<nvar> } ] FROM ]
    { { [ GLOBAL ] <cursor_name> } | @<cursor_variable> }
    [ INTO @<variable_name> { , @<variable_name> } ]

<close_cursor> ::=
    CLOSE { { [ GLOBAL ] <cursor_name> } | <cursor_variable> }

<deallocate_cursor> ::=
    DEALLOCATE { { [ GLOBAL ] <cursor_name> } | @<cursor_variable> }

<cursor_status> ::=
    @@CURSOR_ROWS | @@FETCH_STATUS
```

### 8. Transaction Management

```ebnf
<transaction_statement> ::=
    <begin_transaction>
  | <commit_transaction>
  | <rollback_transaction>
  | <save_transaction>
  | <set_transaction>

<begin_transaction> ::=
    BEGIN { TRAN | TRANSACTION } 
    [ { <transaction_name> | @<tran_name_variable> } [ WITH MARK [ '<description>' ] ] ]

<commit_transaction> ::=
    COMMIT [ { TRAN | TRANSACTION } [ <transaction_name> | @<tran_name_variable> ] ]
    [ WITH ( DELAYED_DURABILITY = { OFF | ON } ) ]  /* SQL Server 2014+ */

<rollback_transaction> ::=
    ROLLBACK [ { TRAN | TRANSACTION } 
    [ <transaction_name> | @<tran_name_variable> | <savepoint_name> | @<savepoint_variable> ] ]

<save_transaction> ::=
    SAVE { TRAN | TRANSACTION } { <savepoint_name> | @<savepoint_variable> }

<set_transaction> ::=
    SET TRANSACTION ISOLATION LEVEL 
    { READ UNCOMMITTED | READ COMMITTED | REPEATABLE READ | SNAPSHOT | SERIALIZABLE }
  | SET IMPLICIT_TRANSACTIONS { ON | OFF }
  | SET XACT_ABORT { ON | OFF }
```

### 9. Error Handling

```ebnf
<error_functions> ::=
    ERROR_NUMBER() | ERROR_SEVERITY() | ERROR_STATE()
  | ERROR_PROCEDURE() | ERROR_LINE() | ERROR_MESSAGE()

<try_catch_functions> ::=
    XACT_STATE() | @@TRANCOUNT

<error_handling> ::=
    BEGIN TRY
        -- Statements
    END TRY
    BEGIN CATCH
        -- Error handling
        [ THROW ; ]  /* Re-throw error */
    END CATCH
```

### 10. Dynamic SQL

```ebnf
<execute_string> ::=
    { EXEC | EXECUTE } ( { @<string_variable> | '<tsql_string>' } [ + ... ] )
    [ [ AS ] { LOGIN | USER } = '<name>' ]
    [ AT { <linked_server_name> | [ <data_source> ] } ]
    [ ; ]

<sp_executesql> ::=
    [ { EXEC | EXECUTE } ] sp_executesql 
    { @<statement> | N'<tsql_string>' } 
    [ , { @<params> | N'@<parameter_name> <data_type> [ OUT | OUTPUT ] { , ... }' } ]
    [ , { @<param1> = <value1> [ OUT | OUTPUT ] } { , ... } ]
```

### 11. Temporary Objects

```ebnf
<temporary_table> ::=
    CREATE TABLE { #<local_temp_table> | ##<global_temp_table> } ...

<table_variable> ::=
    DECLARE @<table_variable> TABLE <table_type_definition>

<common_table_expression> ::=
    WITH [ XMLNAMESPACES , ] [ <common_table_expression> { , ... } ]
    <cte_name> [ ( <column_name> { , <column_name> } ) ]
    AS ( <cte_query_definition> )
```

### 12. XML Support

```ebnf
<xml_data_type_methods> ::=
    <xml_instance>.<method_name> ( <parameters> )

<xml_methods> ::=
    query ( '<XQuery>' )
  | value ( '<XQuery>' , '<sql_type>' )
  | exist ( '<XQuery>' )
  | modify ( '<XML_DML>' )
  | nodes ( '<XQuery>' ) AS <table_alias> ( <column_alias> )

<for_xml_clause> ::=
    FOR XML { RAW [ ( '<element_name>' ) ] | AUTO | EXPLICIT | PATH [ ( '<element_name>' ) ] }
    [ , { XMLDATA | XMLSCHEMA [ ( '<target_namespace>' ) ] } ]
    [ , ELEMENTS [ { XSINIL | ABSENT } ] ]
    [ , ROOT [ ( '<root_name>' ) ] ]
    [ , TYPE ]
    [ , BINARY BASE64 ]

<openxml_clause> ::=
    OPENXML ( <idoc> , '<rowpattern>' [ , <flags> ] )
    [ WITH ( { <schema_declaration> | <table_name> } ) ]
```

### 13. JSON Support (SQL Server 2016+)

```ebnf
<json_functions> ::=
    ISJSON ( <expression> )
  | JSON_VALUE ( <expression> , '<path>' )
  | JSON_QUERY ( <expression> [ , '<path>' ] )
  | JSON_MODIFY ( <expression> , '<path>' , <new_value> )
  | OPENJSON ( <expression> [ , '<path>' ] ) [ WITH ( <schema_definition> ) ]
  | JSON_PATH_EXISTS ( <expression> , '<path>' )  /* SQL Server 2022+ */

<for_json_clause> ::=
    FOR JSON { AUTO | PATH }
    [ , ROOT [ ( '<root_name>' ) ] ]
    [ , INCLUDE_NULL_VALUES ]
    [ , WITHOUT_ARRAY_WRAPPER ]

<json_path> ::= '$' [ <path_element> { <path_element> } ]
<path_element> ::= '.' <member> | '[' <index> ']' | '.*'
```

### 14. Window Functions

```ebnf
<window_function> ::=
    <window_function_name> ( [ <expression> { , <expression> } ] ) 
    OVER ( [ <partition_by_clause> ] [ <order_by_clause> ] [ <frame_clause> ] )

<window_function_name> ::=
    -- Ranking functions
    ROW_NUMBER | RANK | DENSE_RANK | NTILE
    -- Aggregate functions
  | SUM | AVG | MIN | MAX | COUNT | COUNT_BIG 
  | STDEV | STDEVP | VAR | VARP
  | CHECKSUM_AGG | GROUPING | GROUPING_ID
  | STRING_AGG  /* SQL Server 2017+ */
  | APPROX_COUNT_DISTINCT  /* SQL Server 2019+ */
    -- Analytic functions
  | LAG | LEAD | FIRST_VALUE | LAST_VALUE 
  | PERCENT_RANK | CUME_DIST | PERCENTILE_CONT | PERCENTILE_DISC

<partition_by_clause> ::=
    PARTITION BY <expression> { , <expression> }

<order_by_clause> ::=
    ORDER BY <expression> [ { ASC | DESC } ] { , ... }

<frame_clause> ::=
    { ROWS | RANGE } { <frame_start> | BETWEEN <frame_start> AND <frame_end> }

<frame_start> ::=
    UNBOUNDED PRECEDING | <unsigned_value> PRECEDING | CURRENT ROW

<frame_end> ::=
    UNBOUNDED FOLLOWING | <unsigned_value> FOLLOWING | CURRENT ROW
```

### 15. PIVOT and UNPIVOT

```ebnf
<pivot_clause> ::=
    PIVOT ( <aggregate_function> ( <value_column> )
    FOR <pivot_column> IN ( <column_list> ) ) [ AS ] <alias>

<unpivot_clause> ::=
    UNPIVOT ( <value_column> FOR <pivot_column> IN ( <column_list> ) ) [ AS ] <alias>
```

### 16. OUTPUT Clause

```ebnf
<output_clause> ::=
    OUTPUT { <output_column> { , <output_column> } }
    [ INTO { @<table_variable> | <output_table> } [ ( <column_list> ) ] ]

<output_column> ::=
    { DELETED | INSERTED | <from_table_name> } . { * | <column_name> }
  | <scalar_expression> [ [ AS ] <column_alias> ]
  | $action  /* MERGE statement only */
```

### 17. Service Broker

```ebnf
<create_message_type> ::=
    CREATE MESSAGE TYPE <message_type_name>
    [ AUTHORIZATION <owner_name> ]
    [ VALIDATION = { NONE | EMPTY | WELL_FORMED_XML 
                   | VALID_XML WITH SCHEMA COLLECTION <schema_collection> } ]

<create_contract> ::=
    CREATE CONTRACT <contract_name>
    [ AUTHORIZATION <owner_name> ]
    ( <message_type_name> SENT BY { INITIATOR | TARGET | ANY } { , ... } )

<create_queue> ::=
    CREATE QUEUE [ <schema_name> . ] <queue_name>
    [ WITH { STATUS = { ON | OFF } 
           | RETENTION = { ON | OFF }
           | ACTIVATION ( <activation_options> )
           | MAX_QUEUE_READERS = <max_readers>
           | PROCEDURE_NAME = <procedure_name>
           | EXECUTE AS { SELF | '<user_name>' | OWNER } } { , ... } ]
    [ ON { <filegroup> | DEFAULT } ]

<send_statement> ::=
    SEND ON CONVERSATION <conversation_handle>
    [ MESSAGE TYPE <message_type_name> ]
    [ ( <message_body_expression> ) ]

<receive_statement> ::=
    [ WAITFOR ( ] RECEIVE [ TOP ( <n> ) ] <column_specifier> { , ... }
    FROM <queue_name>
    [ INTO @<table_variable> ]
    [ WHERE <where_clause> ] [ ) [ , TIMEOUT <timeout> ] ]
```

### 18. Full-Text Search

```ebnf
<contains_predicate> ::=
    CONTAINS ( { <column_name> | ( <column_list> ) | * } , '<contains_search_condition>' )

<freetext_predicate> ::=
    FREETEXT ( { <column_name> | ( <column_list> ) | * } , '<freetext_string>' )

<contains_search_condition> ::=
    { <simple_term>
    | <prefix_term>
    | <generation_term>
    | <proximity_term>
    | <weighted_term>
    | { ( <contains_search_condition> ) { AND | & | AND NOT | &! | OR | | } <contains_search_condition> } }

<containstable_function> ::=
    CONTAINSTABLE ( <table_name> , { <column_name> | ( <column_list> ) | * } , 
                    '<contains_search_condition>' [ , <top_n_by_rank> ] )

<freetexttable_function> ::=
    FREETEXTTABLE ( <table_name> , { <column_name> | ( <column_list> ) | * } ,
                    '<freetext_string>' [ , <top_n_by_rank> ] )

<semantic_functions> ::=
    SEMANTICKEYPHRASETABLE | SEMANTICSIMILARITYDETAILSTABLE | SEMANTICSIMILARITYTABLE
```

### 19. Spatial Data (SQL Server 2008+)

```ebnf
<spatial_methods> ::=
    -- Geometry methods
    STArea() | STLength() | STDistance ( <geometry> ) | STIntersects ( <geometry> )
  | STContains ( <geometry> ) | STWithin ( <geometry> ) | STTouches ( <geometry> )
  | STOverlaps ( <geometry> ) | STCrosses ( <geometry> ) | STDisjoint ( <geometry> )
  | STEquals ( <geometry> ) | STIntersection ( <geometry> ) | STUnion ( <geometry> )
  | STDifference ( <geometry> ) | STSymDifference ( <geometry> ) | STBuffer ( <distance> )
  | STConvexHull() | STCentroid() | STPointOnSurface() | STExteriorRing()
  | STInteriorRingN ( <index> ) | STNumInteriorRing() | STPointN ( <index> )
  | STStartPoint() | STEndPoint() | STX | STY | STZ | STM
  | STIsValid() | STIsSimple() | STIsEmpty() | STIsClosed() | STIsRing()
  | STNumPoints() | STNumGeometries() | STGeometryN ( <index> )
  | STAsText() | STAsBinary() | STGeomFromText ( <wkt> , <srid> )
  | STGeomFromWKB ( <wkb> , <srid> ) | STSrid

<spatial_aggregates> ::=
    CollectionAggregate | ConvexHullAggregate | EnvelopeAggregate | UnionAggregate
```

### 20. Temporal Tables (SQL Server 2016+)

```ebnf
<temporal_table_clause> ::=
    PERIOD FOR SYSTEM_TIME ( <start_column> , <end_column> )

<system_versioning_clause> ::=
    WITH ( SYSTEM_VERSIONING = ON 
         [ ( HISTORY_TABLE = <schema_name>.<history_table_name> 
             [ , DATA_CONSISTENCY_CHECK = { ON | OFF } ]
             [ , HISTORY_RETENTION_PERIOD = { INFINITE | <number> { DAY | DAYS | WEEK | WEEKS 
                                                                   | MONTH | MONTHS | YEAR | YEARS } } ] ) ] )

<temporal_query_clause> ::=
    FOR SYSTEM_TIME { AS OF <date_time>
                    | FROM <start_date_time> TO <end_date_time>
                    | BETWEEN <start_date_time> AND <end_date_time>
                    | CONTAINED IN ( <start_date_time> , <end_date_time> )
                    | ALL }
```

### 21. Row-Level Security (SQL Server 2016+)

```ebnf
<create_security_policy> ::=
    CREATE SECURITY POLICY <policy_name>
    [ { ADD | ALTER } [ FILTER | BLOCK ] PREDICATE <function_name> ( <column_list> ) 
      ON <table_name> [ , ... ] ]
    [ WITH ( STATE = { ON | OFF } 
           [ , SCHEMABINDING = { ON | OFF } ] ) ]

<security_predicate> ::=
    { FILTER | BLOCK } PREDICATE <function_name> ( <column_list> ) 
    ON <table_name> [ AFTER { INSERT | UPDATE } ]
```

### 22. Dynamic Data Masking (SQL Server 2016+)

```ebnf
<data_masking> ::=
    MASKED WITH ( FUNCTION = '<masking_function>' )

<masking_function> ::=
    'default()' 
  | 'default(<default_value>)'
  | 'email()'
  | 'random(<start>, <end>)'
  | 'partial(<prefix>, <padding>, <suffix>)'
  | 'datetime(<format>)'  /* SQL Server 2022+ */
```

### 23. Graph Tables (SQL Server 2017+)

```ebnf
<create_graph_table> ::=
    CREATE TABLE <table_name> ( <column_definition> { , ... } ) AS { NODE | EDGE }

<graph_predicates> ::=
    MATCH ( <graph_search_pattern> )

<graph_search_pattern> ::=
    <node_alias> { - ( <edge_alias> ) -> <node_alias> }
  | <node_alias> { <- ( <edge_alias> ) - <node_alias> }
  | <graph_search_pattern> { AND | OR } <graph_search_pattern>
  | ( <graph_search_pattern> )

<graph_functions> ::=
    NODE_ID_FROM_PARTS ( <object_id> , <graph_id> )  /* SQL Server 2019+ */
  | OBJECT_ID_FROM_NODE_ID ( <node_id> )  /* SQL Server 2019+ */
  | GRAPH_ID_FROM_NODE_ID ( <node_id> )  /* SQL Server 2019+ */
  | EDGE_ID_FROM_PARTS ( <object_id> , <graph_id> )  /* SQL Server 2019+ */
  | OBJECT_ID_FROM_EDGE_ID ( <edge_id> )  /* SQL Server 2019+ */
  | GRAPH_ID_FROM_EDGE_ID ( <edge_id> )  /* SQL Server 2019+ */

<shortest_path> ::=
    SHORTEST_PATH ( <node_alias> { ( - ( <edge_alias> ) -> <node_alias> ) + } )  /* SQL Server 2019+ */
```

### 24. Machine Learning Services

```ebnf
<sp_execute_external_script> ::=
    EXEC sp_execute_external_script
    @language = N'{ Python | R | Java }'
    , @script = N'<script>'
    [ , @input_data_1 = N'<input_query>' ]
    [ , @input_data_1_name = N'<input_name>' ]
    [ , @output_data_1_name = N'<output_name>' ]
    [ , @parallel = { 0 | 1 } ]
    [ , @params = N'<parameter_definition>' ]
    [ , <parameter_name> = <value> { , ... } ]
    [ WITH RESULT SETS ( <result_sets_definition> ) ]

<create_external_language> ::=
    CREATE EXTERNAL LANGUAGE <language_name>
    [ AUTHORIZATION <owner_name> ]
    FROM { <file_spec> { , <file_spec> } | <content_spec> }  /* SQL Server 2019+ */

<predict_function> ::=
    PREDICT ( MODEL = @<model_variable> , DATA = <table_source> )  /* SQL Server 2017+ */
    [ WITH ( <runtime_parameter> = <value> { , ... } ) ]
```

### 25. Query Store

```ebnf
<alter_database_query_store> ::=
    ALTER DATABASE <database_name>
    SET QUERY_STORE { = { ON | OFF } 
                    | ( <query_store_option> { , ... } ) 
                    | CLEAR [ ALL ] }

<query_store_option> ::=
    OPERATION_MODE = { READ_ONLY | READ_WRITE }
  | CLEANUP_POLICY = ( STALE_QUERY_THRESHOLD_DAYS = <number> )
  | DATA_FLUSH_INTERVAL_SECONDS = <number>
  | INTERVAL_LENGTH_MINUTES = <number>
  | MAX_STORAGE_SIZE_MB = <number>
  | QUERY_CAPTURE_MODE = { ALL | AUTO | NONE | CUSTOM }  /* CUSTOM: SQL Server 2019+ */
  | QUERY_CAPTURE_POLICY = ( <capture_policy_option> { , ... } )  /* SQL Server 2019+ */
  | SIZE_BASED_CLEANUP_MODE = { AUTO | OFF }
  | MAX_PLANS_PER_QUERY = <number>
  | WAIT_STATS_CAPTURE_MODE = { ON | OFF }  /* SQL Server 2017+ */
```

### 26. PolyBase (SQL Server 2016+)

```ebnf
<create_external_data_source> ::=
    CREATE EXTERNAL DATA SOURCE <data_source_name>
    WITH ( TYPE = { HADOOP | SHARD_MAP_MANAGER | RDBMS | BLOB_STORAGE }
         , LOCATION = '<location>'
         [ , RESOURCE_MANAGER_LOCATION = '<resource_manager>' ]
         [ , CREDENTIAL = <credential_name> ]
         [ , DATABASE_NAME = '<database>' ]
         [ , SHARD_MAP_NAME = '<shard_map>' ] )

<create_external_table> ::=
    CREATE EXTERNAL TABLE [ <schema_name> . ] <table_name>
    ( <column_definition> { , ... } )
    WITH ( LOCATION = '<folder_or_filepath>'
         , DATA_SOURCE = <data_source_name>
         , FILE_FORMAT = <file_format_name>
         [ , REJECT_TYPE = { VALUE | PERCENTAGE } ]
         [ , REJECT_VALUE = <value> ]
         [ , REJECT_SAMPLE_VALUE = <value> ] )

<create_external_file_format> ::=
    CREATE EXTERNAL FILE FORMAT <file_format_name>
    WITH ( FORMAT_TYPE = { PARQUET | ORC | RCFILE | DELIMITEDTEXT | JSON | DELTA }  /* DELTA: SQL Server 2022+ */
         [ , <format_options> ] )
```

### 27. Intelligent Query Processing (SQL Server 2017+)

```ebnf
<database_scoped_configuration> ::=
    ALTER DATABASE SCOPED CONFIGURATION 
    { SET <option_name> = { ON | OFF | PRIMARY }
    | CLEAR PROCEDURE_CACHE 
    | FOR SECONDARY SET <option_name> = { ON | OFF | PRIMARY } }

<intelligent_qp_options> ::=
    BATCH_MODE_ADAPTIVE_JOINS  /* SQL Server 2017+ */
  | BATCH_MODE_MEMORY_GRANT_FEEDBACK  /* SQL Server 2017+ */
  | BATCH_MODE_ON_ROWSTORE  /* SQL Server 2019+ */
  | DEFERRED_COMPILATION_TV  /* SQL Server 2019+ */
  | INTERLEAVED_EXECUTION_TVF  /* SQL Server 2017+ */
  | LAST_QUERY_PLAN_STATS  /* SQL Server 2019+ */
  | LIGHTWEIGHT_QUERY_PROFILING  /* SQL Server 2019+ */
  | ROW_MODE_MEMORY_GRANT_FEEDBACK  /* SQL Server 2019+ */
  | SCALAR_UDF_INLINING  /* SQL Server 2019+ */
  | T_MIN_MAX_CLUSTERCOLUMNSTORE_SKIP  /* SQL Server 2019+ */
  | TSQL_SCALAR_UDF_INLINING  /* SQL Server 2019+ */
  | APPROX_PERCENTILE_CONT  /* SQL Server 2022+ */
  | APPROX_PERCENTILE_DISC  /* SQL Server 2022+ */
  | OPTIMIZED_PLAN_FORCING  /* SQL Server 2022+ */
  | PARAMETER_SENSITIVE_PLAN_OPTIMIZATION  /* SQL Server 2022+ */
```

### 28. Ledger Tables (SQL Server 2022+)

```ebnf
<create_ledger_table> ::=
    CREATE TABLE <table_name> ( <column_definition> { , ... } )
    WITH ( LEDGER = ON [ ( <ledger_option> { , ... } ) ] )

<ledger_option> ::=
    LEDGER_VIEW = <schema_name>.<ledger_view_name> 
        [ ( TRANSACTION_ID_COLUMN_NAME = <column_name>
          , SEQUENCE_NUMBER_COLUMN_NAME = <column_name>  
          , OPERATION_TYPE_COLUMN_NAME = <column_name>
          , OPERATION_TYPE_DESC_COLUMN_NAME = <column_name> ) ]
  | APPEND_ONLY = { ON | OFF }

<ledger_verification> ::=
    sp_verify_database_ledger [ @table_name = '<table_name>' ]
                             [ , @verification_option = '<option>' ]

<generate_ledger_digest> ::=
    EXEC sp_generate_database_ledger_digest
```

### 29. Query Hints

```ebnf
<query_hint> ::=
    { FAST <number>
    | FORCE ORDER
    | { LOOP | MERGE | HASH } JOIN
    | EXPAND VIEWS
    | PARAMETERIZATION { SIMPLE | FORCED }
    | RECOMPILE
    | ROBUST PLAN
    | KEEP PLAN
    | KEEPFIXED PLAN
    | MAXDOP <number>
    | MAXRECURSION <number>
    | OPTIMIZE FOR { @<variable> = <literal> } { , ... }
    | OPTIMIZE FOR UNKNOWN
    | NO_PERFORMANCE_SPOOL
    | USE HINT ( '<hint_string>' { , ... } )  /* SQL Server 2016 SP1+ */
    | USE PLAN N'<xml_plan>'
    | TABLE HINT ( <table_alias> , <table_hint> { , ... } )
    | QUERYTRACEON <trace_flag>
    | ASSUME_JOIN_PREDICATE_DEPENDS_ON_FILTERS  /* SQL Server 2019+ */
    | ASSUME_MIN_SELECTIVITY_FOR_FILTER_ESTIMATES  /* SQL Server 2019+ */
    | DISABLE_BATCH_MODE_ADAPTIVE_JOINS  /* SQL Server 2017+ */
    | DISABLE_BATCH_MODE_MEMORY_GRANT_FEEDBACK  /* SQL Server 2017+ */
    | DISABLE_DEFERRED_COMPILATION_TV  /* SQL Server 2019+ */
    | DISABLE_INTERLEAVED_EXECUTION_TVF  /* SQL Server 2017+ */
    | DISABLE_OPTIMIZED_NESTED_LOOP  /* SQL Server 2019+ */
    | DISABLE_OPTIMIZER_ROWGOAL  /* SQL Server 2016+ */
    | DISABLE_PARAMETER_SNIFFING
    | DISABLE_ROW_MODE_MEMORY_GRANT_FEEDBACK  /* SQL Server 2019+ */
    | DISABLE_TSQL_SCALAR_UDF_INLINING  /* SQL Server 2019+ */
    | DISALLOW_BATCH_MODE  /* SQL Server 2019+ */
    | ENABLE_HIST_AMENDMENT_FOR_ASC_KEYS  /* SQL Server 2019+ */
    | ENABLE_QUERY_OPTIMIZER_HOTFIXES
    | FORCE_DEFAULT_CARDINALITY_ESTIMATION  /* SQL Server 2016+ */
    | FORCE_LEGACY_CARDINALITY_ESTIMATION  /* SQL Server 2016+ */
    | QUERY_PLAN_PROFILE  /* SQL Server 2019+ */ }
```

### 30. System Functions and Variables

```ebnf
<system_function> ::=
    -- Metadata functions
    @@DBTS | @@LANGID | @@LANGUAGE | @@LOCK_TIMEOUT | @@MAX_CONNECTIONS
  | @@MAX_PRECISION | @@NESTLEVEL | @@OPTIONS | @@REMSERVER | @@SERVERNAME
  | @@SERVICENAME | @@SPID | @@TEXTSIZE | @@VERSION
  | APP_NAME() | APPLOCK_MODE() | APPLOCK_TEST() | ASSEMBLYPROPERTY()
  | COL_LENGTH() | COL_NAME() | COLUMNPROPERTY() | DATABASE_PRINCIPAL_ID()
  | DATABASEPROPERTYEX() | DB_ID() | DB_NAME() | FILE_ID() | FILE_IDEX()
  | FILE_NAME() | FILEGROUP_ID() | FILEGROUP_NAME() | FILEGROUPPROPERTY()
  | FILEPROPERTY() | FILEPROPERTYEX() | FULLTEXTCATALOGPROPERTY()
  | FULLTEXTSERVICEPROPERTY() | INDEX_COL() | INDEXKEY_PROPERTY()
  | INDEXPROPERTY() | IS_MEMBER() | IS_ROLEMEMBER() | IS_SRVROLEMEMBER()
  | OBJECT_DEFINITION() | OBJECT_ID() | OBJECT_NAME() | OBJECT_SCHEMA_NAME()
  | OBJECTPROPERTY() | OBJECTPROPERTYEX() | ORIGINAL_DB_NAME()
  | PARSENAME() | PERMISSIONS() | PWDCOMPARE() | PWDENCRYPT()
  | QUOTENAME() | SCHEMA_ID() | SCHEMA_NAME() | SCOPE_IDENTITY()
  | SERVERPROPERTY() | SESSION_USER | SESSIONPROPERTY() | STATS_DATE()
  | SUSER_ID() | SUSER_NAME() | SUSER_SID() | SUSER_SNAME()
  | SYSTEM_USER | TYPE_ID() | TYPE_NAME() | TYPEPROPERTY()
  | USER_ID() | USER_NAME()
    -- Security functions
  | CERTENCODED() | CERTPRIVATEKEY() | CURRENT_USER | HAS_PERMS_BY_NAME()
  | IS_OBJECTSIGNED() | IS_ROLEMEMBER() | IS_SRVROLEMEMBER()
  | LOGINPROPERTY() | ORIGINAL_LOGIN() | PERMISSIONS()
  | SESSION_USER | SUSER_ID() | SUSER_NAME() | SUSER_SID()
  | SUSER_SNAME() | SYSTEM_USER | USER_ID() | USER_NAME()
    -- System statistical functions
  | @@CONNECTIONS | @@CPU_BUSY | @@IDLE | @@IO_BUSY | @@PACK_RECEIVED
  | @@PACK_SENT | @@PACKET_ERRORS | @@TIMETICKS | @@TOTAL_ERRORS
  | @@TOTAL_READ | @@TOTAL_WRITE | FN_VIRTUALFILESTATS()
    -- Configuration functions
  | @@DATEFIRST | @@DBTS | @@LANGID | @@LANGUAGE | @@LOCK_TIMEOUT
  | @@MAX_CONNECTIONS | @@MAX_PRECISION | @@NESTLEVEL | @@OPTIONS
  | @@REMSERVER | @@SERVERNAME | @@SERVICENAME | @@SPID
  | @@TEXTSIZE | @@VERSION
```

### 31. Operators

```ebnf
<operator> ::=
    -- Arithmetic
    + | - | * | / | %

    -- Bitwise
    & | | | ^ | ~

    -- Comparison
    = | > | < | >= | <= | <> | != | !< | !>

    -- Logical
    ALL | AND | ANY | BETWEEN | EXISTS | IN | LIKE | NOT | OR | SOME

    -- String concatenation
    +

    -- Compound assignment
    += | -= | *= | /= | %= | &= | ^= | |=

    -- Unary
    + | - | ~

    -- Scope resolution
    ::
```

### 32. Special Constructs

```ebnf
<case_expression> ::=
    CASE { <input_expression> 
           WHEN <when_expression> THEN <result_expression> { ... }
         | WHEN <boolean_expression> THEN <result_expression> { ... } }
    [ ELSE <else_result_expression> ] END

<iif_function> ::=
    IIF ( <boolean_expression> , <true_value> , <false_value> )  /* SQL Server 2012+ */

<choose_function> ::=
    CHOOSE ( <index> , <value1> , <value2> { , ... } )  /* SQL Server 2012+ */

<nullif_expression> ::=
    NULLIF ( <expression> , <expression> )

<coalesce_expression> ::=
    COALESCE ( <expression> { , <expression> } )

<format_function> ::=
    FORMAT ( <value> , '<format>' [ , '<culture>' ] )  /* SQL Server 2012+ */

<concat_function> ::=
    CONCAT ( <string_value1> , <string_value2> { , ... } )  /* SQL Server 2012+ */
  | CONCAT_WS ( '<separator>' , <string_value1> , <string_value2> { , ... } )  /* SQL Server 2017+ */

<string_split_function> ::=
    STRING_SPLIT ( <string> , '<separator>' [ , <enable_ordinal> ] )  /* SQL Server 2016+, ordinal: 2022+ */

<string_agg_function> ::=
    STRING_AGG ( <expression> , '<separator>' ) [ WITHIN GROUP ( ORDER BY <order_list> ) ]  /* SQL Server 2017+ */

<trim_function> ::=
    TRIM ( [ [ { LEADING | TRAILING | BOTH } ] [ <characters> ] FROM ] <string> )  /* SQL Server 2017+ */

<translate_function> ::=
    TRANSLATE ( <input_string> , '<characters>' , '<translations>' )  /* SQL Server 2017+ */

<approx_count_distinct> ::=
    APPROX_COUNT_DISTINCT ( <expression> )  /* SQL Server 2019+ */

<datetrunc_function> ::=
    DATETRUNC ( <datepart> , <date> )  /* SQL Server 2022+ */

<date_bucket_function> ::=
    DATE_BUCKET ( <datepart> , <number> , <date> [ , <origin> ] )  /* SQL Server 2022+ */

<greatest_least_functions> ::=
    GREATEST ( <expression> { , <expression> } )  /* SQL Server 2022+ */
  | LEAST ( <expression> { , <expression> } )  /* SQL Server 2022+ */

<bit_manipulation_functions> ::=
    BIT_COUNT ( <expression> )  /* SQL Server 2022+ */
  | GET_BIT ( <expression> , <bit_position> )  /* SQL Server 2022+ */
  | SET_BIT ( <expression> , <bit_position> [ , <bit_value> ] )  /* SQL Server 2022+ */
  | LEFT_SHIFT ( <expression> , <shift_count> )  /* SQL Server 2022+ */
  | RIGHT_SHIFT ( <expression> , <shift_count> )  /* SQL Server 2022+ */
```

## Usage Notes

1. **Version Compatibility**: Features marked with version comments require specific SQL Server versions
2. **Case Sensitivity**: Default collation determines identifier case sensitivity
3. **Identifier Delimiters**: Use square brackets `[ ]` or double quotes `" "` (with QUOTED_IDENTIFIER ON)
4. **Statement Terminators**: Semicolon `;` is optional but recommended
5. **Batch Separators**: `GO` is used by client tools, not part of T-SQL
6. **Comments**: 
   - Single line: `--`
   - Multi-line: `/* ... */`
7. **String Literals**: Use single quotes `'`, double with `''` for embedded quotes
8. **Unicode Strings**: Prefix with `N`, e.g., `N'Unicode string'`
9. **Binary Literals**: Use `0x` prefix, e.g., `0xDEADBEEF`
10. **Money Literals**: Use currency symbol, e.g., `$123.45`
11. **Date/Time Literals**: Various formats supported, ISO 8601 recommended
12. **NULL Handling**: Three-valued logic (TRUE, FALSE, UNKNOWN)
13. **Collations**: Affect string comparisons and sorting
14. **Schemas**: Default is `dbo`, always qualify objects in production code
15. **Deprecated Features**: Avoid TEXT, NTEXT, IMAGE; use (N)VARCHAR(MAX), VARBINARY(MAX)

This comprehensive grammar covers T-SQL from SQL Server 2008 through SQL Server 2022, providing a complete foundation for implementing a T-SQL parser.