# ScratchBird SQL Grammar (BNF/EBNF)

## Overview

ScratchBird SQL uses context-aware parsing to minimize reserved words. Keywords are only reserved in positions where they would create ambiguity.

## Notation

```
::=          Definition
[ ]          Optional (0 or 1)
{ }          Repetition (0 or more)
( )          Grouping
|            Alternative
< >          Non-terminal
UPPERCASE    Terminal (keyword)
'literal'    Terminal (literal)
```

## Top-Level Statements

```bnf
<sql_statement> ::= 
    <ddl_statement>
  | <dml_statement>
  | <dcl_statement>
  | <tcl_statement>
  | <utility_statement>

<statement_list> ::= 
    <sql_statement> [ ';' ] { <sql_statement> [ ';' ] }
```

## DDL (Data Definition Language)

### CREATE Statements

```bnf
<ddl_statement> ::=
    <create_database>
  | <create_schema>
  | <create_table>
  | <create_index>
  | <create_view>
  | <create_procedure>
  | <create_function>
  | <create_trigger>
  | <create_domain>
  | <create_sequence>
  | <alter_statement>
  | <drop_statement>

<create_table> ::=
    CREATE [ TEMPORARY | TEMP ] TABLE [ IF NOT EXISTS ] 
    <table_name> 
    '(' <table_element_list> ')'
    [ <table_options> ]

<table_element_list> ::=
    <table_element> { ',' <table_element> }

<table_element> ::=
    <column_definition>
  | <table_constraint>

<column_definition> ::=
    <column_name> <data_type> 
    { <column_constraint> }

<data_type> ::=
    <predefined_type>
  | <domain_name>
  | <array_type>

<predefined_type> ::=
    SMALLINT | INTEGER | BIGINT | INT128
  | DECIMAL [ '(' <precision> [ ',' <scale> ] ')' ]
  | NUMERIC [ '(' <precision> [ ',' <scale> ] ')' ]
  | REAL | FLOAT | DOUBLE PRECISION
  | CHAR [ '(' <length> ')' ] [ CHARACTER SET <charset> ]
  | VARCHAR '(' <length> ')' [ CHARACTER SET <charset> ]
  | TEXT | BLOB | CLOB
  | DATE | TIME | TIMESTAMP [ WITH TIME ZONE ]
  | BOOLEAN | UUID | JSON | XML
  | UINT8 | UINT16 | UINT32 | UINT64

<array_type> ::=
    <data_type> '[' [ <array_dimension> ] ']'

<column_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    (
        NOT NULL
      | NULL
      | UNIQUE
      | PRIMARY KEY
      | REFERENCES <table_name> [ '(' <column_name> ')' ]
      | CHECK '(' <expression> ')'
      | DEFAULT <default_value>
      | GENERATED ALWAYS AS '(' <expression> ')' [ STORED | VIRTUAL ]
      | AUTO_INCREMENT
    )

<table_constraint> ::=
    [ CONSTRAINT <constraint_name> ]
    (
        PRIMARY KEY '(' <column_list> ')'
      | UNIQUE '(' <column_list> ')'
      | FOREIGN KEY '(' <column_list> ')' 
        REFERENCES <table_name> [ '(' <column_list> ')' ]
        [ <referential_actions> ]
      | CHECK '(' <expression> ')'
    )

<referential_actions> ::=
    [ ON DELETE <referential_action> ]
    [ ON UPDATE <referential_action> ]

<referential_action> ::=
    CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION
```

### CREATE INDEX

```bnf
<create_index> ::=
    CREATE [ UNIQUE ] INDEX [ IF NOT EXISTS ] 
    <index_name> 
    ON <table_name> 
    [ USING <index_method> ]
    '(' <index_element_list> ')'
    [ WHERE <expression> ]
    [ <index_options> ]

<index_method> ::=
    BTREE | HASH | GIN | GIST | RTREE | BITMAP | LSM | COLUMNSTORE

<index_element_list> ::=
    <index_element> { ',' <index_element> }

<index_element> ::=
    <column_name> [ <collation> ] [ ASC | DESC ] [ NULLS { FIRST | LAST } ]
  | '(' <expression> ')' [ <collation> ] [ ASC | DESC ]
```

### CREATE VIEW

```bnf
<create_view> ::=
    CREATE [ OR REPLACE ] [ TEMPORARY | TEMP ] VIEW 
    <view_name> [ '(' <column_list> ')' ]
    AS <select_statement>
    [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]
```

### CREATE PROCEDURE/FUNCTION

```bnf
<create_procedure> ::=
    CREATE [ OR REPLACE ] PROCEDURE 
    <procedure_name> 
    '(' [ <parameter_list> ] ')'
    [ RETURNS <data_type> ]
    [ <routine_characteristics> ]
    <routine_body>

<create_function> ::=
    CREATE [ OR REPLACE ] FUNCTION 
    <function_name> 
    '(' [ <parameter_list> ] ')'
    RETURNS <data_type>
    [ <routine_characteristics> ]
    <routine_body>

<parameter_list> ::=
    <parameter> { ',' <parameter> }

<parameter> ::=
    [ IN | OUT | INOUT ] <parameter_name> <data_type> [ DEFAULT <expression> ]

<routine_characteristics> ::=
    { LANGUAGE { SQL | PSQL | PYTHON | JAVASCRIPT } 
    | DETERMINISTIC | NOT DETERMINISTIC
    | CONTAINS SQL | NO SQL | READS SQL DATA | MODIFIES SQL DATA
    | SECURITY { DEFINER | INVOKER }
    | PARALLEL { SAFE | RESTRICTED | UNSAFE }
    }

<routine_body> ::=
    <sql_statement>
  | <compound_statement>
  | EXTERNAL NAME <external_reference>
```

### CREATE TRIGGER

```bnf
<create_trigger> ::=
    CREATE [ OR REPLACE ] TRIGGER 
    <trigger_name>
    { BEFORE | AFTER } 
    { INSERT | UPDATE [ OF <column_list> ] | DELETE | SELECT }
    [ OR { INSERT | UPDATE | DELETE | SELECT } ]
    ON <table_name>
    [ REFERENCING { OLD [ AS ] <identifier> | NEW [ AS ] <identifier> } ]
    [ FOR EACH { ROW | STATEMENT } ]
    [ POSITION <integer> ]
    [ WHEN '(' <expression> ')' ]
    <trigger_body>

<trigger_body> ::=
    <sql_statement>
  | <compound_statement>
```

## DML (Data Manipulation Language)

### SELECT Statement

```bnf
<dml_statement> ::=
    <select_statement>
  | <insert_statement>
  | <update_statement>
  | <delete_statement>
  | <merge_statement>

<select_statement> ::=
    [ <with_clause> ]
    <select_expression>
    [ <order_by_clause> ]
    [ <limit_clause> ]
    [ <for_clause> ]

<with_clause> ::=
    WITH [ RECURSIVE ] 
    <cte> { ',' <cte> }

<cte> ::=
    <cte_name> [ '(' <column_list> ')' ] 
    AS '(' <select_statement> ')'

<select_expression> ::=
    <select_term> { <set_operator> <select_term> }

<select_term> ::=
    SELECT 
    [ ALL | DISTINCT [ ON '(' <expression_list> ')' ] ]
    <select_list>
    [ FROM <from_clause> ]
    [ WHERE <expression> ]
    [ GROUP BY <grouping_element_list> ]
    [ HAVING <expression> ]
    [ WINDOW <window_definition_list> ]

<set_operator> ::=
    UNION [ ALL | DISTINCT ]
  | INTERSECT [ ALL | DISTINCT ]
  | EXCEPT [ ALL | DISTINCT ]

<select_list> ::=
    '*' 
  | <select_item> { ',' <select_item> }

<select_item> ::=
    <expression> [ [ AS ] <alias> ]
  | <table_name> '.' '*'

<from_clause> ::=
    <from_item> { ',' <from_item> }

<from_item> ::=
    <table_reference> [ <join_clause> ]

<table_reference> ::=
    <table_name> [ [ AS ] <alias> ]
  | '(' <select_statement> ')' [ AS ] <alias>
  | <table_function> [ [ AS ] <alias> ]
  | LATERAL <from_item>

<join_clause> ::=
    { <join_type> JOIN <table_reference> <join_condition> }

<join_type> ::=
    [ INNER | { LEFT | RIGHT | FULL } [ OUTER ] | CROSS ]

<join_condition> ::=
    ON <expression>
  | USING '(' <column_list> ')'
```

### INSERT Statement

```bnf
<insert_statement> ::=
    [ <with_clause> ]
    INSERT INTO <table_name> 
    [ '(' <column_list> ')' ]
    { 
        VALUES <value_list> { ',' <value_list> }
      | <select_statement>
      | DEFAULT VALUES
    }
    [ ON CONFLICT <conflict_clause> ]
    [ RETURNING <select_list> ]

<value_list> ::=
    '(' <expression_list> ')'

<conflict_clause> ::=
    '(' <column_list> ')' 
    DO { NOTHING | UPDATE SET <update_list> [ WHERE <expression> ] }
```

### UPDATE Statement

```bnf
<update_statement> ::=
    [ <with_clause> ]
    UPDATE <table_name> [ [ AS ] <alias> ]
    SET <update_list>
    [ FROM <from_clause> ]
    [ WHERE <expression> ]
    [ RETURNING <select_list> ]

<update_list> ::=
    <update_item> { ',' <update_item> }

<update_item> ::=
    <column_name> '=' <expression>
  | '(' <column_list> ')' '=' '(' <expression_list> ')'
```

### DELETE Statement

```bnf
<delete_statement> ::=
    [ <with_clause> ]
    DELETE FROM <table_name> [ [ AS ] <alias> ]
    [ USING <from_clause> ]
    [ WHERE <expression> ]
    [ RETURNING <select_list> ]
```

## Expressions

```bnf
<expression> ::=
    <logical_or_expression>

<logical_or_expression> ::=
    <logical_and_expression> { OR <logical_and_expression> }

<logical_and_expression> ::=
    <logical_not_expression> { AND <logical_not_expression> }

<logical_not_expression> ::=
    [ NOT ] <comparison_expression>

<comparison_expression> ::=
    <additive_expression> [ <comparison_operator> <additive_expression> ]

<comparison_operator> ::=
    '=' | '<>' | '!=' | '<' | '>' | '<=' | '>=' 
  | LIKE | NOT LIKE 
  | ILIKE | NOT ILIKE
  | SIMILAR TO | NOT SIMILAR TO
  | BETWEEN | NOT BETWEEN
  | IN | NOT IN
  | EXISTS | NOT EXISTS
  | IS NULL | IS NOT NULL
  | IS DISTINCT FROM | IS NOT DISTINCT FROM

<additive_expression> ::=
    <multiplicative_expression> { ( '+' | '-' | '||' ) <multiplicative_expression> }

<multiplicative_expression> ::=
    <unary_expression> { ( '*' | '/' | '%' ) <unary_expression> }

<unary_expression> ::=
    [ '+' | '-' ] <postfix_expression>

<postfix_expression> ::=
    <primary_expression> 
    { 
        '[' <expression> ']'                    -- Array subscript
      | '.' <identifier>                        -- Field access
      | '(' [ <expression_list> ] ')'           -- Function call
      | '::' <data_type>                        -- Cast
      | COLLATE <collation_name>                -- Collation
    }

<primary_expression> ::=
    <literal>
  | <column_reference>
  | <parameter_reference>
  | '(' <expression> ')'
  | <case_expression>
  | <cast_expression>
  | <window_function>
  | <aggregate_function>
  | <subquery>

<literal> ::=
    <numeric_literal>
  | <string_literal>
  | <boolean_literal>
  | <null_literal>
  | <date_time_literal>
  | <interval_literal>
  | <array_literal>
  | <json_literal>

<case_expression> ::=
    CASE 
    { WHEN <expression> THEN <expression> }
    [ ELSE <expression> ]
    END
  | CASE <expression>
    { WHEN <expression> THEN <expression> }
    [ ELSE <expression> ]
    END

<cast_expression> ::=
    CAST '(' <expression> AS <data_type> ')'
```

## Window Functions

```bnf
<window_function> ::=
    <window_function_name> '(' [ <expression_list> ] ')' 
    OVER <window_specification>

<window_specification> ::=
    '(' 
    [ PARTITION BY <expression_list> ]
    [ ORDER BY <order_by_list> ]
    [ <window_frame> ]
    ')'
  | <window_name>

<window_frame> ::=
    { RANGE | ROWS | GROUPS }
    { 
        BETWEEN <window_bound> AND <window_bound>
      | <window_bound>
    }

<window_bound> ::=
    UNBOUNDED PRECEDING
  | <expression> PRECEDING
  | CURRENT ROW
  | <expression> FOLLOWING
  | UNBOUNDED FOLLOWING
```

## Compound Statements (PSQL)

```bnf
<compound_statement> ::=
    [ <label> ':' ]
    BEGIN 
        [ <declaration_list> ]
        <statement_list>
        [ <exception_handler_list> ]
    END [ <label> ]

<declaration_list> ::=
    { <declaration> ';' }

<declaration> ::=
    DECLARE <variable_list> <data_type> [ DEFAULT <expression> ]
  | DECLARE <cursor_name> CURSOR FOR <select_statement>

<statement_list> ::=
    { <psql_statement> ';' }

<psql_statement> ::=
    <sql_statement>
  | <assignment_statement>
  | <if_statement>
  | <case_statement>
  | <loop_statement>
  | <leave_statement>
  | <return_statement>
  | <signal_statement>
  | <call_statement>

<assignment_statement> ::=
    SET <variable_name> '=' <expression>
  | <variable_name> ':=' <expression>

<if_statement> ::=
    IF <expression> THEN 
        <statement_list>
    { ELSEIF <expression> THEN <statement_list> }
    [ ELSE <statement_list> ]
    END IF

<loop_statement> ::=
    [ <label> ':' ]
    { WHILE <expression> DO | REPEAT | LOOP | FOR <for_spec> DO }
        <statement_list>
    { END WHILE | UNTIL <expression> END REPEAT | END LOOP }
    [ <label> ]

<for_spec> ::=
    <variable_name> IN <expression> '..' <expression>
  | <cursor_name>
  | <select_statement>
```

## Transaction Control (TCL)

```bnf
<tcl_statement> ::=
    START TRANSACTION [ <transaction_mode> ]
  | BEGIN [ WORK ] [ <transaction_mode> ]
  | COMMIT [ WORK ] [ AND [ NO ] CHAIN ]
  | ROLLBACK [ WORK ] [ AND [ NO ] CHAIN ] [ TO [ SAVEPOINT ] <savepoint_name> ]
  | SAVEPOINT <savepoint_name>
  | RELEASE [ SAVEPOINT ] <savepoint_name>

<transaction_mode> ::=
    { ISOLATION LEVEL <isolation_level> 
    | READ WRITE | READ ONLY
    | [ NOT ] DEFERRABLE
    }

<isolation_level> ::=
    READ UNCOMMITTED 
  | READ COMMITTED 
  | REPEATABLE READ 
  | SERIALIZABLE
```

## Access Control (DCL)

```bnf
<dcl_statement> ::=
    <grant_statement>
  | <revoke_statement>
  | <create_user>
  | <create_role>
  | <alter_user>
  | <drop_user>
  | <drop_role>

<grant_statement> ::=
    GRANT <privilege_list> 
    ON <object_type> <object_name> 
    TO <grantee_list> 
    [ WITH GRANT OPTION ]

<revoke_statement> ::=
    REVOKE [ GRANT OPTION FOR ] <privilege_list>
    ON <object_type> <object_name>
    FROM <grantee_list>
    [ CASCADE | RESTRICT ]

<privilege_list> ::=
    ALL [ PRIVILEGES ]
  | <privilege> { ',' <privilege> }

<privilege> ::=
    SELECT | INSERT | UPDATE | DELETE | TRUNCATE
  | REFERENCES | TRIGGER | CREATE | CONNECT
  | TEMPORARY | EXECUTE | USAGE
```

## Utility Statements

```bnf
<utility_statement> ::=
    <show_statement>
  | <describe_statement>
  | <explain_statement>
  | <analyze_statement>
  | <vacuum_statement>
  | <copy_statement>
  | <set_statement>

<show_statement> ::=
    SHOW DATABASES
  | SHOW SCHEMAS [ FROM <database_name> ]
  | SHOW TABLES [ FROM <schema_name> ]
  | SHOW COLUMNS FROM <table_name>
  | SHOW INDEX FROM <table_name>
  | SHOW CREATE TABLE <table_name>
  | SHOW PROCESSLIST
  | SHOW VARIABLES [ LIKE <pattern> ]

<explain_statement> ::=
    EXPLAIN [ ANALYZE ] [ VERBOSE ] <sql_statement>

<set_statement> ::=
    SET [ SESSION | LOCAL ] <variable_name> { TO | '=' } <value>
  | SET TIME ZONE <timezone>
  | SET SCHEMA <schema_name>
  | SET ROLE <role_name>
```

## Identifiers and Names

```bnf
<identifier> ::=
    <regular_identifier>
  | <delimited_identifier>

<regular_identifier> ::=
    <letter> { <letter> | <digit> | '_' }

<delimited_identifier> ::=
    '"' <any_character_except_quote> '"'
  | '`' <any_character_except_backtick> '`'
  | '[' <any_character_except_bracket> ']'

<qualified_name> ::=
    [ <schema_name> '.' ] <object_name>
  | <database_name> '.' <schema_name> '.' <object_name>
```

## Context-Aware Parsing Rules

### Reserved Word Minimization

Keywords are only reserved in specific contexts:

1. **Always Reserved**: 
   - SELECT, FROM, WHERE, INSERT, UPDATE, DELETE
   - CREATE, ALTER, DROP
   - AND, OR, NOT

2. **Context-Dependent**:
   - TIMESTAMP: Reserved as type, not as identifier
   - DATE/TIME: Reserved as type, not as column name
   - COUNT/SUM/AVG: Reserved as function, not as identifier

3. **Never Reserved**:
   - Common column names: id, name, value, data, status
   - Common table names: users, orders, items

### Statement Termination

Statements end when:
1. Semicolon encountered
2. New statement keyword at column 1
3. EOF reached
4. GO command (MSSQL compatibility)

### Comment Syntax

```bnf
<comment> ::=
    '--' <any_text_until_newline>
  | '/*' <any_text> '*/'
  | '//' <any_text_until_newline>  -- C-style
```

## Operator Precedence (Highest to Lowest)

1. `::` (cast), `.` (member), `[]` (subscript)
2. `+` `-` (unary)
3. `*` `/` `%`
4. `+` `-` (binary)
5. `||` (concatenation)
6. Comparison operators
7. `NOT`
8. `AND`
9. `OR`

## Lexical Elements

```bnf
<numeric_literal> ::=
    <integer> [ '.' <integer> ] [ E [ '+' | '-' ] <integer> ]

<string_literal> ::=
    '\'' { <any_character_except_quote> | '\'\'' } '\''
  | E'\'' { <escape_sequence> } '\''  -- Escape strings

<boolean_literal> ::=
    TRUE | FALSE

<null_literal> ::=
    NULL

<parameter_reference> ::=
    '?' | ':' <identifier> | '$' <integer>
```

## Notes

1. This grammar supports multiple SQL dialects through the Y-Valve
2. Context-aware parsing reduces reserved words significantly
3. Statement termination is automatic in most cases
4. Unicode identifiers are fully supported
5. Case sensitivity follows database configuration