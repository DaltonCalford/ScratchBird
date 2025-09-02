### MySQL/MariaDB SQL Variations — EBNF Report

Version scope: MySQL 8.0.x and MariaDB 10.6–11.x family features. Focus on dialect differences vs SQL Standard and between MySQL and MariaDB. Grammar aims to provide a parser blueprint; tokenization and version-conditional features may require flags.

EBNF index:
- Tokens and lexical peculiarities
- Expressions and precedence (with MySQL boolean pragmatics)
- Queries (SELECT, set ops, CTEs [MySQL 8], windowing)
- DML (INSERT/REPLACE/UPSERT, UPDATE, DELETE) with RETURNING [MariaDB/MySQL 8.0.19+ limited]
- DDL (CREATE/ALTER/DROP TABLE) with engine/charset/collations/partitions
- Indexes, constraints, sequences/auto_increment, views
- Routines (FUNCTION/PROCEDURE, triggers, events), partitions
- Utility (EXPLAIN, ANALYZE, OPTIMIZE, SHOW, SET, GRANT)
- Highlights and differences vs PostgreSQL/SQL Server

Tokens and lexical peculiarities
```ebnf
identifier              ::= unquoted_identifier | quoted_identifier | backtick_identifier ;
unquoted_identifier     ::= letter { letter | digit | '_' | '$' } ;
quoted_identifier       ::= '"' { qchar } '"' ;
backtick_identifier     ::= '`' { bchar } '`' ;

letter                  ::= 'A'..'Z' | 'a'..'z' | '_' ;
digit                   ::= '0'..'9' ;
qchar                   ::= any_character_except('"') | '""' ;
bchar                   ::= any_character_except('`') | '``' ;

string_literal          ::= '\'' { schar } '\'' | '"' { qchar } '"' ;
schar                   ::= any_character_except('\'', '\\') | '\\' any_character ;

bit_literal             ::= 'b' '\'' { '0' | '1' } '\'' | '0b' { '0' | '1' } ;
hex_literal             ::= 'x' '\'' { hex_digit } '\'' | '0x' { hex_digit } ;
hex_digit               ::= digit | 'A'..'F' | 'a'..'f' ;

numeric_literal         ::= integer_literal | decimal_literal | float_literal ;
integer_literal         ::= digit { digit } ;
decimal_literal         ::= digit { digit } '.' { digit } | '.' digit { digit } ;
float_literal           ::= ( integer_literal | decimal_literal ) ( 'E' | 'e' ) [ '+' | '-' ] integer_literal ;

boolean_literal         ::= 'TRUE' | 'FALSE' ;
null_literal            ::= 'NULL' ;

parameter_reference     ::= '?' | ':' identifier ;

collation_name          ::= identifier ;
charset_name            ::= identifier ;
```

Expressions and precedence
```ebnf
expression              ::= or_expression ;
or_expression           ::= xor_expression { 'OR' xor_expression } ;
xor_expression          ::= and_expression { 'XOR' and_expression } ;
and_expression          ::= not_expression { 'AND' not_expression } ;
not_expression          ::= [ 'NOT' | '!' ] comparison_expression ;

comparison_expression   ::= add_expression comparison_tail? ;
comparison_tail         ::= comparison_operator add_expression
                          | [ 'NOT' ] 'BETWEEN' add_expression 'AND' add_expression
                          | [ 'NOT' ] 'IN' '(' in_list_or_subquery ')'
                          | [ 'NOT' ] 'LIKE' add_expression [ 'ESCAPE' add_expression ]
                          | [ 'NOT' ] 'REGEXP' add_expression
                          | 'IS' [ 'NOT' ] ( 'NULL' | 'TRUE' | 'FALSE' | 'UNKNOWN' ) ;

comparison_operator     ::= '=' | '<=>' | '!=' | '<>' | '<' | '<=' | '>' | '>=' ;

in_list_or_subquery    ::= expression { ',' expression } | select_statement ;

add_expression         ::= mult_expression { ( '+' | '-' ) mult_expression } ;
mult_expression        ::= unary_expression { ( '*' | '/' | 'DIV' | '%' | 'MOD' ) unary_expression } ;

unary_expression       ::= [ '+' | '-' | '!' | '~' | 'BINARY' ] postfix_expression
                         | 'EXISTS' '(' select_statement ')'
                         | 'CASE' case_operand? when_clauses [ 'ELSE' expression ] 'END' ;

case_operand           ::= expression ;
when_clauses           ::= 'WHEN' expression 'THEN' expression { 'WHEN' expression 'THEN' expression } ;

postfix_expression     ::= primary_expression postfix_suffix* ;
postfix_suffix         ::= array_subscript | field_access | type_cast | collate_suffix ;
array_subscript        ::= '[' expression ']' ;
field_access           ::= '.' identifier ;
type_cast              ::= 'CAST' '(' expression 'AS' type_name ')' ;
collate_suffix         ::= 'COLLATE' collation_name ;

primary_expression     ::= literal | parameter_reference | column_reference | function_call | select_subquery | '(' expression ')' ;
literal                ::= numeric_literal | string_literal | hex_literal | bit_literal | boolean_literal | null_literal ;
column_reference       ::= identifier [ '.' identifier ] [ '.' identifier ] ;

function_call          ::= function_name '(' [ function_args ] ')' window_clause? ;
function_name          ::= identifier [ '.' identifier ] ;
function_args          ::= [ 'DISTINCT' ] expression { ',' expression } | named_args ;
named_args             ::= arg_assignment { ',' arg_assignment } ;
arg_assignment         ::= identifier '=>' expression ;

window_clause          ::= 'OVER' ( window_name | window_spec ) ;
window_name            ::= identifier ;
window_spec            ::= '(' [ 'PARTITION' 'BY' expression_list ] [ 'ORDER' 'BY' order_list ] [ frame_clause ] ')' ;
expression_list        ::= expression { ',' expression } ;
order_list             ::= order_item { ',' order_item } ;
order_item             ::= expression [ 'ASC' | 'DESC' ] ;

frame_clause           ::= frame_type frame_range ;
frame_type             ::= 'RANGE' | 'ROWS' ;
frame_range            ::= frame_start | 'BETWEEN' frame_start 'AND' frame_end ;
frame_start            ::= 'UNBOUNDED' 'PRECEDING' | expression 'PRECEDING' | 'CURRENT' 'ROW' ;
frame_end              ::= 'CURRENT' 'ROW' | expression 'FOLLOWING' | 'UNBOUNDED' 'FOLLOWING' ;

type_name              ::= identifier [ '(' expression_list ')' ] [ 'UNSIGNED' ] [ 'ZEROFILL' ] ;
```

Queries (SELECT, set operations, CTE)
```ebnf
select_statement       ::= [ with_clause ] select_core { set_operation select_core } [ order_by_clause ] [ limit_clause ] [ lock_clause ] ;

with_clause            ::= 'WITH' [ 'RECURSIVE' ] cte_definition { ',' cte_definition } ;
cte_definition         ::= identifier [ '(' identifier { ',' identifier } ')' ] 'AS' '(' select_core { set_operation select_core } ')' ;

set_operation          ::= 'UNION' [ 'ALL' | 'DISTINCT' ] | 'INTERSECT' [ 'ALL' ] | 'EXCEPT' [ 'ALL' ] ;

select_core            ::= 'SELECT' [ 'ALL' | 'DISTINCT' ] select_list from_clause? where_clause? group_by_clause? having_clause? window_definitions? ;
select_list            ::= select_item { ',' select_item } ;
select_item            ::= expression [ alias ] | '*' | qualified_star ;
qualified_star         ::= identifier '.' '*' | identifier '.' identifier '.' '*' ;
alias                  ::= [ 'AS' ] identifier ;

from_clause            ::= 'FROM' table_reference { ',' table_reference } ;
table_reference        ::= table_factor joined_tail* ;
table_factor           ::= relation_expr [ alias ] [ index_hint ]
                         | '(' select_statement ')' [ alias ]
                         | '(' table_reference ')' ;
relation_expr          ::= identifier [ '.' identifier ] [ '.' identifier ] ;
index_hint             ::= 'USE' 'INDEX' '(' index_list ')' | 'IGNORE' 'INDEX' '(' index_list ')' | 'FORCE' 'INDEX' '(' index_list ')' ;
index_list             ::= identifier { ',' identifier } ;

joined_tail            ::= join_op table_factor [ join_condition ] ;
join_op                ::= 'JOIN' | 'INNER' 'JOIN' | 'LEFT' [ 'OUTER' ] 'JOIN' | 'RIGHT' [ 'OUTER' ] 'JOIN' | 'CROSS' 'JOIN' | 'STRAIGHT_JOIN' ;
join_condition         ::= 'ON' expression | 'USING' '(' identifier { ',' identifier } ')' ;

where_clause           ::= 'WHERE' expression ;
group_by_clause        ::= 'GROUP' 'BY' expression_list [ 'WITH' 'ROLLUP' ] ;
having_clause          ::= 'HAVING' expression ;

window_definitions     ::= 'WINDOW' window_def { ',' window_def } ;
window_def             ::= window_name 'AS' window_spec ;

order_by_clause        ::= 'ORDER' 'BY' order_list ;
limit_clause           ::= 'LIMIT' ( integer_literal [ ',' integer_literal ] | integer_literal 'OFFSET' integer_literal ) ;
lock_clause            ::= 'FOR' ( 'UPDATE' | 'SHARE' ) ;
```

DML: INSERT/REPLACE/UPSERT, UPDATE, DELETE
```ebnf
insert_statement       ::= 'INSERT' [ 'IGNORE' ] 'INTO' relation_expr [ '(' column_list ')' ] insert_source [ on_duplicate_clause ] [ returning_clause ] ;
replace_statement      ::= 'REPLACE' 'INTO' relation_expr [ '(' column_list ')' ] insert_source [ returning_clause ] ;
column_list            ::= identifier { ',' identifier } ;
insert_source          ::= 'VALUES' values_list | select_statement | 'SET' set_assign_list ;
values_list            ::= '(' value_list ')' { ',' '(' value_list ')' } ;
value_list             ::= expression { ',' expression } ;
set_assign_list        ::= set_assign { ',' set_assign } ;
set_assign             ::= identifier '=' expression ;

on_duplicate_clause    ::= 'ON' 'DUPLICATE' 'KEY' 'UPDATE' set_assign_list ;
returning_clause       ::= 'RETURNING' select_list ;

update_statement       ::= 'UPDATE' [ 'LOW_PRIORITY' ] [ 'IGNORE' ] table_reference 'SET' set_assign_list [ where_clause ] [ order_by_clause ] [ limit_clause ] ;
delete_statement       ::= 'DELETE' [ 'LOW_PRIORITY' ] [ 'QUICK' ] [ 'IGNORE' ] 'FROM' relation_expr [ using_join ] [ where_clause ] [ order_by_clause ] [ limit_clause ] ;
using_join             ::= 'USING' table_reference ;
```

DDL: CREATE/ALTER/DROP TABLE, partitions, engine/charset
```ebnf
create_table          ::= 'CREATE' [ 'TEMPORARY' ] 'TABLE' [ 'IF' 'NOT' 'EXISTS' ] relation_expr '(' table_element { ',' table_element } ')' table_options? [ partition_clause ] ;
table_element         ::= column_definition | table_constraint ;
column_definition     ::= identifier type_name column_attributes? ;
column_attributes     ::= { nullability | default_clause | auto_increment | comment | column_format | storage | collate_suffix | generated_column } ;
nullability           ::= 'NULL' | 'NOT' 'NULL' ;
default_clause        ::= 'DEFAULT' expression ;
auto_increment        ::= 'AUTO_INCREMENT' ;
comment               ::= 'COMMENT' string_literal ;
column_format         ::= 'COLUMN_FORMAT' ( 'FIXED' | 'DYNAMIC' | 'DEFAULT' ) ;
storage               ::= 'STORAGE' ( 'DISK' | 'MEMORY' | 'DEFAULT' ) ;
generated_column      ::= 'GENERATED' 'ALWAYS' 'AS' '(' expression ')' ( 'VIRTUAL' | 'STORED' )
                        | 'AS' '(' expression ')' ( 'VIRTUAL' | 'PERSISTENT' | 'STORED' ) ;

table_constraint      ::= [ 'CONSTRAINT' identifier ] ( primary_key | unique_key | foreign_key | check_constraint ) ;
primary_key           ::= 'PRIMARY' 'KEY' '(' index_col_list ')' index_options? ;
unique_key            ::= 'UNIQUE' [ 'KEY' | 'INDEX' ] [ identifier ] '(' index_col_list ')' index_options? ;
foreign_key           ::= 'FOREIGN' 'KEY' [ identifier ] '(' column_list ')' 'REFERENCES' relation_expr '(' column_list ')' reference_options? ;
check_constraint      ::= 'CHECK' '(' expression ')' ;

index_col_list        ::= index_col { ',' index_col } ;
index_col             ::= identifier [ '(' integer_literal ')' ] [ 'ASC' | 'DESC' ] ;
index_options         ::= { 'USING' identifier | 'KEY_BLOCK_SIZE' '=' integer_literal | 'WITH' 'PARSER' identifier | 'COMMENT' string_literal | 'VISIBLE' | 'INVISIBLE' } ;

table_options         ::= { engine | charset | collate | row_format | auto_increment_opt | comment | compression | encryption } ;
engine                ::= 'ENGINE' '=' identifier ;
charset               ::= ( 'DEFAULT' )? ( 'CHARSET' | 'CHARACTER' 'SET' ) '=' charset_name ;
collate               ::= 'COLLATE' '=' collation_name ;
row_format            ::= 'ROW_FORMAT' '=' ( 'DEFAULT' | 'DYNAMIC' | 'FIXED' | 'COMPRESSED' | 'REDUNDANT' | 'COMPACT' ) ;
auto_increment_opt    ::= 'AUTO_INCREMENT' '=' integer_literal ;
compression           ::= 'COMPRESSION' '=' string_literal ;
encryption            ::= 'ENCRYPTION' '=' ( 'Y' | 'N' ) ;

partition_clause      ::= 'PARTITION' 'BY' partition_method ;
partition_method      ::= 'RANGE' '(' partition_expr ')' partition_defs
                        | 'LIST' '(' partition_expr ')' partition_defs
                        | 'HASH' '(' partition_expr ')' partition_defs
                        | 'LINEAR' 'HASH' '(' partition_expr ')' partition_defs
                        | 'KEY' '(' column_list ')' partition_defs ;
partition_expr        ::= expression ;
partition_defs        ::= '(' partition_def { ',' partition_def } ')' ;
partition_def         ::= 'PARTITION' identifier partition_options ;
partition_options     ::= { 'VALUES' ('LESS' 'THAN' '(' expression_list ')' | 'IN' '(' expression_list ')' ) | 'ENGINE' '=' identifier | 'COMMENT' string_literal | 'DATA' 'DIRECTORY' string_literal | 'INDEX' 'DIRECTORY' string_literal } ;

alter_table           ::= 'ALTER' 'TABLE' relation_expr alter_action { ',' alter_action } ;
alter_action          ::= 'ADD' [ 'COLUMN' ] column_definition
                        | 'ADD' table_constraint
                        | 'DROP' [ 'COLUMN' ] identifier
                        | 'DROP' 'PRIMARY' 'KEY'
                        | 'DROP' 'FOREIGN' 'KEY' identifier
                        | 'MODIFY' [ 'COLUMN' ] column_definition
                        | 'CHANGE' [ 'COLUMN' ] identifier column_definition
                        | 'RENAME' [ 'TO' ] relation_expr
                        | 'CONVERT' 'TO' 'CHARACTER' 'SET' charset_name [ 'COLLATE' collation_name ]
                        | 'ALTER' 'PARTITION' partition_alter ;

partition_alter       ::= 'ADD' 'PARTITION' '(' partition_def { ',' partition_def } ')' | 'DROP' 'PARTITION' identifier { ',' identifier } | 'REORGANIZE' 'PARTITION' identifier 'INTO' '(' partition_def { ',' partition_def } ')' ;

drop_table            ::= 'DROP' 'TABLE' [ 'IF' 'EXISTS' ] relation_expr { ',' relation_expr } ;
```

Indexes, sequences/auto_increment, views
```ebnf
create_index          ::= 'CREATE' [ 'UNIQUE' | 'FULLTEXT' | 'SPATIAL' ] 'INDEX' [ identifier ] 'ON' relation_expr '(' index_col_list ')' index_options? ;
drop_index            ::= 'DROP' 'INDEX' identifier 'ON' relation_expr ;

create_view           ::= 'CREATE' [ 'OR' 'REPLACE' ] 'VIEW' relation_expr [ '(' column_list ')' ] 'AS' select_statement [ 'WITH' 'CHECK' 'OPTION' ] ;
drop_view             ::= 'DROP' 'VIEW' [ 'IF' 'EXISTS' ] relation_expr { ',' relation_expr } ;
```

Routines, triggers, events
```ebnf
create_function       ::= 'CREATE' 'FUNCTION' identifier '(' param_list? ')' 'RETURNS' type_name routine_characteristics routine_body ;
create_procedure      ::= 'CREATE' 'PROCEDURE' identifier '(' param_list? ')' routine_characteristics routine_body ;
param_list            ::= param_decl { ',' param_decl } ;
param_decl            ::= [ 'IN' | 'OUT' | 'INOUT' ] identifier type_name ;
routine_characteristics ::= { 'LANGUAGE' 'SQL' | 'DETERMINISTIC' | 'NOT' 'DETERMINISTIC' | 'READS' 'SQL' 'DATA' | 'MODIFIES' 'SQL' 'DATA' | 'CONTAINS' 'SQL' | 'NO' 'SQL' | 'COMMENT' string_literal | 'SQL' 'SECURITY' ( 'DEFINER' | 'INVOKER' ) } ;
routine_body          ::= 'BEGIN' ... 'END' ;

create_trigger        ::= 'CREATE' 'TRIGGER' identifier trigger_timing trigger_event 'ON' relation_expr 'FOR' 'EACH' ( 'ROW' | 'STATEMENT' ) trigger_body ;
trigger_timing        ::= 'BEFORE' | 'AFTER' ;
trigger_event         ::= 'INSERT' | 'UPDATE' | 'DELETE' ;
trigger_body          ::= 'BEGIN' ... 'END' ;

create_event          ::= 'CREATE' 'EVENT' identifier 'ON' 'SCHEDULE' schedule 'DO' routine_body ;
schedule              ::= 'AT' expression | 'EVERY' expression [ 'STARTS' expression ] [ 'ENDS' expression ] ;
```

Utility and session
```ebnf
explain_statement     ::= 'EXPLAIN' [ 'ANALYZE' ] [ 'FORMAT' '=' ( 'TREE' | 'JSON' | 'TRADITIONAL' ) ] ( select_statement | insert_statement | update_statement | delete_statement | replace_statement ) ;
optimize_table        ::= 'OPTIMIZE' 'TABLE' relation_expr { ',' relation_expr } ;
analyze_table         ::= 'ANALYZE' 'TABLE' relation_expr { ',' relation_expr } ;
show_statement        ::= 'SHOW' show_what ;
show_what             ::= 'DATABASES' | 'TABLES' | 'COLUMNS' 'FROM' relation_expr | 'INDEX' 'FROM' relation_expr | 'CREATE' 'TABLE' relation_expr | 'ENGINE' 'INNODB' 'STATUS' | identifier ;
set_statement         ::= 'SET' set_item { ',' set_item } ;
set_item              ::= identifier '=' expression | 'NAMES' charset_name [ 'COLLATE' collation_name ] | 'CHARACTER' 'SET' charset_name ;
grant_statement       ::= 'GRANT' ... ; revoke_statement ::= 'REVOKE' ... ;
```

Highlights and differences
- MySQL NULL-safe `<=>`, `REGEXP`, JSON functions instead of JSON operators; MariaDB varies in native JSON types.
- `REPLACE` is MySQL/MariaDB-specific. UPSERT via `ON DUPLICATE KEY UPDATE` differs from PostgreSQL `ON CONFLICT`.
- Window functions exist; no GROUPS frame. Partial `RETURNING` support varies by version and engine.
- Character sets/collations are pervasive; `CHARSET`, `COLLATE` appear on many objects.
- Partitioning syntax differs from PostgreSQL; storage engines and index visibility are dialect-specific.

