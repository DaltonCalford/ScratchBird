### PostgreSQL SQL Extensions — EBNF Report

Version scope: PostgreSQL 15–16 family features with emphasis on PostgreSQL-specific extensions beyond SQL Standard. This grammar is suitable as a starting point for implementing a full SQL parser. It focuses on statement- and expression-level structure and dials into PostgreSQL-specific clauses and operators. Tokenization and some edge-case ambiguities (especially around user-defined operators) may require additional lexer support.

Notation: EBNF, case-insensitive keywords, `[...]` optional, `{...}` zero-or-more, `|` alternation, `()` grouping. Terminals are quoted with single quotes. Nonterminals are in snake_case. Comments beginning with `--` explain dialect notes.

Implementation notes:
- PostgreSQL allows user-defined operators with flexible symbolic names. Operator tokenization should consider the longest-match rule and the documented operator character classes.
- Identifiers: unquoted identifiers fold to lower case. Quoted identifiers preserve case and allow otherwise-illegal characters.
- String constants use standard conforming strings by default; escape string syntax `E'...'` supports C-style escapes.
- Dollar-quoted strings: `$$...$$` or `$tag$...$tag$` appear in function bodies but may occur in SQL expressions too.
- Many features are gated by object existence (`IF [NOT] EXISTS`) and privilege; grammar admits these regardless of runtime validation.

EBNF index:
- Tokens and lexical categories
- Expressions and operator precedence
- Queries (SELECT, set ops, CTEs, windowing)
- DML (INSERT/UPSERT, UPDATE, DELETE, MERGE) with RETURNING
- DDL (CREATE/ALTER/DROP TABLE, constraints, partitioning, inheritance)
- Indexes, constraints, sequences, types, views/materialized views
- Functions/procedures/triggers, extensions, schemas
- Utility (COPY, EXPLAIN/ANALYZE, VACUUM, ANALYZE, COMMENT, GRANT/REVOKE, SET)

Tokens and lexical categories
```ebnf
identifier           ::= unquoted_identifier | quoted_identifier ;
unquoted_identifier  ::= letter { letter | digit | '_' | '$' } ; -- folds to lower case
quoted_identifier    ::= '"' { qchar } '"' ; -- doubled quotes inside

letter               ::= 'A'..'Z' | 'a'..'z' | '_' ;
digit                ::= '0'..'9' ;
qchar                ::= any_character_except('"') | '""' ;

numeric_literal      ::= integer_literal | decimal_literal | float_literal ;
integer_literal      ::= digit { digit } ;
decimal_literal      ::= digit { digit } '.' { digit } | '.' digit { digit } ;
float_literal        ::= ( integer_literal | decimal_literal ) ( 'E' | 'e' ) [ '+' | '-' ] integer_literal ;

string_literal       ::= quote { schar } quote ;
quote                ::= '\'' ;
schar                ::= any_character_except('\'', '\\') | '\\' any_character ;
escape_string        ::= 'E' string_literal ;
dollar_string        ::= '$' [ tag ] '$' { any_character_until_matching_tag } '$' [ tag ] '$' ;
tag                  ::= letter { letter | digit | '_' } ;

bytea_literal        ::= 'X' '\'' hex_pair { hex_pair } '\'' ;
hex_pair             ::= hex_digit hex_digit ;
hex_digit            ::= digit | 'A'..'F' | 'a'..'f' ;

boolean_literal      ::= 'TRUE' | 'FALSE' ;
null_literal         ::= 'NULL' ;

parameter_reference  ::= '$' integer_literal ;            -- positional: $1, $2
named_parameter      ::= ':' identifier ;                 -- allowed in some clients, not core PG parser

type_identifier      ::= identifier [ '.' identifier ] [ '.' identifier ] ; -- schema/type/attribute
collation_identifier ::= identifier [ '.' identifier ] ;
```

Expressions and operator precedence
```ebnf
expression               ::= or_expression ;

or_expression            ::= and_expression { 'OR' and_expression } ;
and_expression           ::= not_expression { 'AND' not_expression } ;

not_expression           ::= [ 'NOT' ] comparison_expression ;

comparison_expression    ::= add_expression comparison_tail? ;
comparison_tail          ::= comparison_operator add_expression
                           | 'IS' [ 'NOT' ] 'NULL'
                           | 'IS' [ 'NOT' ] 'TRUE'
                           | 'IS' [ 'NOT' ] 'FALSE'
                           | 'IS' [ 'NOT' ] 'UNKNOWN'
                           | 'IS' [ 'NOT' ] 'DISTINCT' 'FROM' add_expression
                           | [ 'NOT' ] 'BETWEEN' add_expression 'AND' add_expression
                           | [ 'NOT' ] 'IN' '(' in_list_or_subquery ')'
                           | [ 'NOT' ] like_op add_expression [ 'ESCAPE' add_expression ]
                           | [ 'NOT' ] 'SIMILAR' 'TO' add_expression [ 'ESCAPE' add_expression ] ;

comparison_operator      ::= '=' | '!=' | '<>' | '<' | '<=' | '>' | '>=' ;
like_op                  ::= 'LIKE' | 'ILIKE' ; -- ILIKE is PostgreSQL-specific

in_list_or_subquery     ::= expression { ',' expression }
                          | select_statement ;

add_expression          ::= mult_expression { ( '+' | '-' ) mult_expression } ;
mult_expression         ::= unary_expression { ( '*' | '/' | '%' ) unary_expression } ;

unary_expression        ::= [ '+' | '-' | '~' ] postfix_expression
                          | 'EXISTS' '(' select_statement ')'
                          | 'CASE' case_operand? when_clauses [ 'ELSE' expression ] 'END' ;

case_operand            ::= expression ;
when_clauses            ::= 'WHEN' expression 'THEN' expression { 'WHEN' expression 'THEN' expression } ;

postfix_expression      ::= primary_expression postfix_suffix* ;
postfix_suffix          ::= array_subscript
                          | field_access
                          | type_cast
                          | 'COLLATE' collation_identifier
                          | 'AT' 'TIME' 'ZONE' expression ;

array_subscript         ::= '[' ( expression [ ':' expression ] ) ']' ; -- slice or single element
field_access            ::= '.' identifier ;                         -- composite fields / JSON path helper in funcs
type_cast               ::= '::' type_name
                          | 'CAST' '(' expression 'AS' type_name ')' ;

primary_expression      ::= literal
                          | parameter_reference
                          | column_reference
                          | row_constructor
                          | array_constructor
                          | function_call
                          | select_subquery
                          | '(' expression ')' ;

literal                 ::= numeric_literal | string_literal | escape_string | dollar_string | boolean_literal | null_literal | bytea_literal ;

column_reference        ::= identifier [ '.' identifier ] [ '.' identifier ] ; -- c.schema.table.column

row_constructor         ::= 'ROW' '(' expression_list ')'
                          | '(' expression_list ')' ; -- ambiguous with grouping; parser resolves contextually
expression_list         ::= expression { ',' expression } ;

array_constructor       ::= 'ARRAY' '[' expression_list ']'
                          | 'ARRAY' '(' select_statement ')' ;

function_call           ::= function_name '(' [ function_args ] ')' window_clause? filter_clause? ;
function_name           ::= identifier [ '.' identifier ] ;
function_args           ::= [ 'DISTINCT' ] expression { ',' expression }
                          | named_args ;
named_args              ::= arg_assignment { ',' arg_assignment } ;
arg_assignment          ::= identifier '=>' expression ;

filter_clause           ::= 'FILTER' '(' 'WHERE' expression ')' ;       -- aggregate FILTER (WHERE ...)
window_clause           ::= 'OVER' ( window_name | window_specification ) ;
window_name             ::= identifier ;
window_specification    ::= '(' [ 'PARTITION' 'BY' expression_list ]
                                 [ 'ORDER' 'BY' order_list ]
                                 [ frame_clause ] ')' ;
order_list              ::= order_item { ',' order_item } ;
order_item              ::= expression [ 'COLLATE' collation_identifier ] [ 'ASC' | 'DESC' ] [ 'NULLS' 'FIRST' | 'NULLS' 'LAST' ] ;

frame_clause            ::= frame_type frame_range [ frame_exclusion ] ;
frame_type              ::= 'RANGE' | 'ROWS' | 'GROUPS' ;
frame_range             ::= frame_start | 'BETWEEN' frame_start 'AND' frame_end ;
frame_start             ::= 'UNBOUNDED' 'PRECEDING'
                          | expression 'PRECEDING'
                          | 'CURRENT' 'ROW' ;
frame_end               ::= 'CURRENT' 'ROW'
                          | expression 'FOLLOWING'
                          | 'UNBOUNDED' 'FOLLOWING' ;
frame_exclusion         ::= 'EXCLUDE' ( 'CURRENT' 'ROW' | 'GROUP' | 'TIES' | 'NO' 'OTHERS' ) ;

type_name               ::= type_identifier [ type_modifiers ] | array_type ;
type_modifiers          ::= '(' expression_list ')' ;
array_type              ::= type_name '[]' ; -- right-recursive array suffixes
```

PostgreSQL-specific operators and constructs (non-exhaustive)
```ebnf
json_operator           ::= '->' | '->>' | '#>' | '#>>' | '@>' | '<@' | '?' | '?|' | '?&' ;
string_operator         ::= '||' ;
range_operator          ::= '<<' | '>>' | '&<' | '&>' | '&&' | '-' | '+' ;
network_operator        ::= '<<' | '<<=' | '>>' | '>>=' | '&&' | '~' | '~*' ;
text_search_operator    ::= '@@' | '@@' '!' | '@@' '?' ;
```

Queries (SELECT, set operations, CTE)
```ebnf
select_statement        ::= [ with_clause ] query_expression [ order_by_clause ] [ limit_offset_clause ] [ locking_clause ] ;

with_clause             ::= 'WITH' [ 'RECURSIVE' ] cte_definition { ',' cte_definition } ;
cte_definition          ::= identifier [ '(' identifier { ',' identifier } ')' ]
                            'AS' [ 'NOT' 'MATERIALIZED' | 'MATERIALIZED' ] '(' query_expression ')' ;

query_expression        ::= select_core { set_operation [ 'ALL' | 'DISTINCT' ] select_core } ;
set_operation           ::= 'UNION' | 'INTERSECT' | 'EXCEPT' ;

select_core             ::= 'SELECT' [ 'ALL' | 'DISTINCT' [ 'ON' '(' expression_list ')' ] ]
                            select_list
                            [ from_clause ]
                            [ where_clause ]
                            [ group_by_clause ]
                            [ having_clause ]
                            [ window_definitions ] ;

select_list             ::= select_item { ',' select_item } ;
select_item             ::= expression [ alias ] | '*' | qualified_star ;
qualified_star          ::= identifier '.' '*' | identifier '.' identifier '.' '*' ;
alias                   ::= [ 'AS' ] identifier ;

from_clause             ::= 'FROM' from_item { ',' from_item } ;
from_item               ::= table_reference
                          | joined_table
                          | lateral_item
                          | function_table
                          | '(' query_expression ')' [ alias ] ;

table_reference         ::= relation_expr [ alias ] [ tablesample ] ;
relation_expr           ::= identifier [ '.' identifier ] [ '.' identifier ] ;

tablesample             ::= 'TABLESAMPLE' method '(' expression_list ')' [ 'REPEATABLE' '(' expression ')' ] ;
method                  ::= identifier ;

lateral_item            ::= 'LATERAL' '(' query_expression ')' [ alias ] ;
function_table          ::= function_call [ 'AS' ] [ alias ] [ '(' column_alias_list ')' ] ;
column_alias_list       ::= identifier { ',' identifier } ;

joined_table            ::= from_item join_tail ;
join_tail               ::= join_op from_item [ join_condition ] { join_op from_item [ join_condition ] } ;
join_op                 ::= [ 'NATURAL' ] ( 'JOIN' | 'INNER' 'JOIN' | 'LEFT' [ 'OUTER' ] 'JOIN' | 'RIGHT' [ 'OUTER' ] 'JOIN' | 'FULL' [ 'OUTER' ] 'JOIN' | 'CROSS' 'JOIN' ) ;
join_condition          ::= 'ON' expression | 'USING' '(' identifier { ',' identifier } ')' ;

where_clause            ::= 'WHERE' expression ;
group_by_clause         ::= 'GROUP' 'BY' group_list ;
group_list              ::= grouping_element { ',' grouping_element } ;
grouping_element        ::= expression | '(' expression_list ')' | 'GROUPING' 'SETS' '(' group_list { ',' group_list } ')' | 'ROLLUP' '(' expression_list ')' | 'CUBE' '(' expression_list ')' ;
having_clause           ::= 'HAVING' expression ;

window_definitions      ::= 'WINDOW' window_def { ',' window_def } ;
window_def              ::= window_name 'AS' window_specification ;

order_by_clause         ::= 'ORDER' 'BY' order_list ;

limit_offset_clause     ::= ( 'LIMIT' expression [ 'OFFSET' expression ] )
                          | ( 'OFFSET' expression [ 'LIMIT' expression ] )
                          | ( 'FETCH' ( 'FIRST' | 'NEXT' ) [ expression ] ( 'ROW' | 'ROWS' ) ( 'ONLY' | 'WITH' 'TIES' ) ) ;

locking_clause          ::= ( 'FOR' lock_strength [ 'OF' identifier { ',' identifier } ] [ 'NOWAIT' | 'SKIP' 'LOCKED' ] ) { ',' 'FOR' lock_strength [ 'OF' identifier { ',' identifier } ] [ 'NOWAIT' | 'SKIP' 'LOCKED' ] } ;
lock_strength           ::= 'UPDATE' | 'NO' 'KEY' 'UPDATE' | 'SHARE' | 'KEY' 'SHARE' ;
```

DML: INSERT / UPSERT, UPDATE, DELETE, MERGE (PostgreSQL 15+)
```ebnf
insert_statement        ::= 'INSERT' 'INTO' relation_expr [ '(' column_list ')' ]
                            [ 'OVERRIDING' ( 'SYSTEM' | 'USER' ) 'VALUE' ]
                            insert_source
                            [ on_conflict_clause ]
                            [ returning_clause ] ;

column_list             ::= identifier { ',' identifier } ;
insert_source           ::= 'DEFAULT' 'VALUES'
                          | 'VALUES' values_list
                          | select_statement ;
values_list             ::= '(' value_list ')' { ',' '(' value_list ')' } ;
value_list              ::= expression { ',' expression } ;

on_conflict_clause      ::= 'ON' 'CONFLICT' ( '(' conflict_target ')' [ where_clause ] | 'ON' 'CONSTRAINT' identifier | )
                            'DO' ( 'NOTHING' | 'UPDATE' 'SET' set_clause_list [ where_clause ] ) ;
conflict_target         ::= index_expr_list [ 'INCLUDE' '(' column_list ')' ] ;
index_expr_list         ::= index_expr { ',' index_expr } ;
index_expr              ::= expression [ 'COLLATE' collation_identifier ] [ opclass ] [ 'ASC' | 'DESC' ] [ 'NULLS' 'FIRST' | 'LAST' ] ;
opclass                 ::= identifier [ '(' identifier { ',' identifier } ')' ] ;

returning_clause        ::= 'RETURNING' ( '*' | select_list ) ;

update_statement        ::= 'UPDATE' [ 'ONLY' ] relation_expr [ '*' ] [ alias ]
                            'SET' set_clause_list
                            [ from_clause ]
                            [ where_clause ]
                            [ returning_clause ] ;
set_clause_list         ::= set_clause { ',' set_clause } ;
set_clause              ::= identifier [ '.' identifier ] '=' expression ;

delete_statement        ::= 'DELETE' 'FROM' [ 'ONLY' ] relation_expr [ '*' ] [ alias ]
                            [ using_clause ]
                            [ where_clause | 'WHERE' 'CURRENT' 'OF' identifier ]
                            [ returning_clause ] ;
using_clause            ::= 'USING' from_item { ',' from_item } ;

merge_statement         ::= 'MERGE' 'INTO' relation_expr [ alias ]
                            'USING' ( relation_expr | '(' select_statement ')' [ alias ] )
                            'ON' expression
                            merge_when_clause { merge_when_clause }
                            [ returning_clause ] ;
merge_when_clause       ::= 'WHEN' 'MATCHED' [ 'AND' expression ] 'THEN' merge_action
                          | 'WHEN' 'NOT' 'MATCHED' [ 'AND' expression ] 'THEN' merge_action ;
merge_action            ::= 'DO' 'NOTHING'
                          | 'UPDATE' 'SET' set_clause_list [ where_clause ]
                          | 'INSERT' '(' column_list ')' 'VALUES' '(' value_list ')' [ where_clause ] ;
```

DDL: CREATE/ALTER/DROP TABLE, constraints, inheritance, partitioning
```ebnf
create_table           ::= 'CREATE' [ 'TEMP' | 'TEMPORARY' | 'UNLOGGED' ] 'TABLE' [ 'IF' 'NOT' 'EXISTS' ] relation_expr
                           '(' table_element { ',' table_element } ')'
                           [ table_inherits ]
                           [ partition_by ]
                           [ 'WITH' '(' storage_parameter_list ')' ]
                           [ 'TABLESPACE' identifier ] ;

table_element          ::= column_definition | table_constraint ;
column_definition      ::= identifier type_name
                           [ 'COLLATE' collation_identifier ]
                           { column_constraint } ;

storage_parameter_list ::= storage_parameter { ',' storage_parameter } ;
storage_parameter      ::= identifier '=' expression ;

column_constraint      ::= 'CONSTRAINT' identifier column_constraint_body | column_constraint_body ;
column_constraint_body ::= 'NOT' 'NULL'
                         | 'NULL'
                         | 'DEFAULT' expression
                         | 'GENERATED' ( 'ALWAYS' | 'BY' 'DEFAULT' ) 'AS' 'IDENTITY' [ '(' identity_options ')' ]
                         | 'GENERATED' 'ALWAYS' 'AS' '(' expression ')' 'STORED'
                         | 'PRIMARY' 'KEY'
                         | 'UNIQUE'
                         | 'REFERENCES' relation_expr [ '(' column_list ')' ] [ match_type ] [ referential_actions ]
                         | 'CHECK' '(' expression ')' [ 'NO' 'INHERIT' ] ;

identity_options       ::= identity_option { ',' identity_option } ;
identity_option        ::= 'START' [ 'WITH' ] integer_literal
                         | 'INCREMENT' [ 'BY' ] integer_literal
                         | 'MINVALUE' integer_literal | 'NO' 'MINVALUE'
                         | 'MAXVALUE' integer_literal | 'NO' 'MAXVALUE'
                         | 'CACHE' integer_literal
                         | 'CYCLE' | 'NO' 'CYCLE'
                         | 'OWNED' 'BY' ( relation_expr '.' identifier | 'NONE' ) ;

match_type             ::= 'MATCH' ( 'FULL' | 'PARTIAL' | 'SIMPLE' ) ;
referential_actions    ::= [ 'ON' 'DELETE' ref_action ] [ 'ON' 'UPDATE' ref_action ] [ 'DEFERRABLE' | 'NOT' 'DEFERRABLE' ] [ 'INITIALLY' ( 'DEFERRED' | 'IMMEDIATE' ) ] ;
ref_action             ::= 'NO' 'ACTION' | 'RESTRICT' | 'CASCADE' | 'SET' 'NULL' | 'SET' 'DEFAULT' ;

table_constraint       ::= [ 'CONSTRAINT' identifier ] table_constraint_body ;
table_constraint_body  ::= 'PRIMARY' 'KEY' '(' index_expr_list ')'
                         | 'UNIQUE' '(' index_expr_list ')'
                         | 'CHECK' '(' expression ')' [ 'NO' 'INHERIT' ]
                         | 'EXCLUDE' [ 'USING' index_method ] '(' exclude_element { ',' exclude_element } ')' [ 'WHERE' '(' expression ')' ] ;
exclude_element        ::= index_expr 'WITH' operator_name [ 'ASC' | 'DESC' ] [ 'NULLS' 'FIRST' | 'LAST' ] ;
operator_name          ::= identifier [ '.' identifier ] ;
index_method           ::= identifier ;

table_inherits         ::= 'INHERITS' '(' relation_expr { ',' relation_expr } ')' ;
partition_by           ::= 'PARTITION' 'BY' partition_method ;
partition_method       ::= 'RANGE' '(' index_expr_list ')' | 'LIST' '(' index_expr_list ')' | 'HASH' '(' index_expr_list ')' ;

alter_table            ::= 'ALTER' 'TABLE' [ 'IF' 'EXISTS' ] relation_expr alter_table_action { ',' alter_table_action } ;
alter_table_action     ::= 'ADD' [ 'COLUMN' ] column_definition
                         | 'ADD' table_constraint
                         | 'DROP' [ 'COLUMN' ] [ 'IF' 'EXISTS' ] identifier [ 'RESTRICT' | 'CASCADE' ]
                         | 'DROP' [ 'CONSTRAINT' ] [ 'IF' 'EXISTS' ] identifier [ 'RESTRICT' | 'CASCADE' ]
                         | 'ALTER' [ 'COLUMN' ] identifier alter_column_action
                         | 'RENAME' [ 'COLUMN' ] identifier 'TO' identifier
                         | 'RENAME' 'TO' identifier
                         | 'SET' 'SCHEMA' identifier
                         | 'ATTACH' 'PARTITION' relation_expr 'FOR' 'VALUES' partition_bound_spec
                         | 'DETACH' 'PARTITION' relation_expr [ 'CONCURRENTLY' ] [ 'FINALIZE' ] ;

alter_column_action    ::= 'SET' 'DEFAULT' expression
                         | 'DROP' 'DEFAULT'
                         | 'SET' 'NOT' 'NULL' | 'DROP' 'NOT' 'NULL'
                         | 'TYPE' type_name [ 'USING' expression ]
                         | 'ADD' 'GENERATED' 'ALWAYS' 'AS' '(' expression ')' 'STORED'
                         | 'SET' 'STATISTICS' integer_literal
                         | 'SET' 'IDENTITY' '(' identity_options ')'
                         | 'ADD' 'IDENTITY' [ '(' identity_options ')' ]
                         | 'DROP' 'IDENTITY' ;

partition_bound_spec   ::= 'IN' '(' expression_list ')' | 'FROM' '(' expression_list ')' 'TO' '(' expression_list ')' | 'WITH' '(' 'MODULUS' integer_literal ',' 'REMAINDER' integer_literal ')' ;

drop_table             ::= 'DROP' 'TABLE' [ 'IF' 'EXISTS' ] relation_expr { ',' relation_expr } [ 'CASCADE' | 'RESTRICT' ] ;
```

Indexes, sequences, types, views/materialized views
```ebnf
create_index           ::= 'CREATE' [ 'UNIQUE' ] 'INDEX' [ 'CONCURRENTLY' ] [ 'IF' 'NOT' 'EXISTS' ] identifier
                           'ON' relation_expr [ 'USING' index_method ] '(' index_element { ',' index_element } ')'
                           [ 'INCLUDE' '(' column_list ')' ]
                           [ 'WHERE' expression ]
                           [ 'TABLESPACE' identifier ] ;
index_element          ::= index_expr [ opclass ] [ 'ASC' | 'DESC' ] [ 'NULLS' 'FIRST' | 'LAST' ] ;

drop_index             ::= 'DROP' 'INDEX' [ 'CONCURRENTLY' ] [ 'IF' 'EXISTS' ] identifier { ',' identifier } [ 'CASCADE' | 'RESTRICT' ] ;

create_sequence        ::= 'CREATE' 'SEQUENCE' [ 'IF' 'NOT' 'EXISTS' ] relation_expr [ sequence_options ] ;
sequence_options       ::= sequence_option { sequence_option } ;
sequence_option        ::= 'AS' type_name | 'INCREMENT' [ 'BY' ] integer_literal | 'MINVALUE' integer_literal | 'NO' 'MINVALUE' | 'MAXVALUE' integer_literal | 'NO' 'MAXVALUE' | 'START' [ 'WITH' ] integer_literal | 'CACHE' integer_literal | 'CYCLE' | 'OWNED' 'BY' ( relation_expr '.' identifier | 'NONE' ) ;

create_type            ::= 'CREATE' 'TYPE' identifier ( 'AS' enum_type | 'AS' range_type | 'AS' composite_type ) ;
enum_type              ::= 'ENUM' '(' string_literal { ',' string_literal } ')' ;
range_type             ::= 'RANGE' '(' 'SUBTYPE' '=' type_name { ',' range_option } ')' ;
range_option           ::= 'COLLATION' '=' collation_identifier | 'SUBTYPE_OPCLASS' '=' opclass | 'CANONICAL' '=' function_name | 'SUBTYPE_DIFF' '=' function_name ;
composite_type         ::= '(' identifier type_name { ',' identifier type_name } ')' ;

create_view            ::= 'CREATE' [ 'OR' 'REPLACE' ] [ 'TEMP' | 'TEMPORARY' ] 'VIEW' [ 'IF' 'NOT' 'EXISTS' ] relation_expr [ '(' column_list ')' ] 'AS' select_statement [ 'WITH' '(' view_option_list ')' ] ;
view_option_list       ::= view_option { ',' view_option } ;
view_option            ::= 'CHECK' 'OPTION' [ 'LOCAL' | 'CASCADED' ] ;

create_materialized_view ::= 'CREATE' 'MATERIALIZED' 'VIEW' [ 'IF' 'NOT' 'EXISTS' ] relation_expr [ '(' column_list ')' ] 'AS' select_statement [ 'WITH' 'NO' 'DATA' | 'WITH' 'DATA' ] ;
refresh_materialized_view ::= 'REFRESH' 'MATERIALIZED' 'VIEW' [ 'CONCURRENTLY' ] relation_expr [ 'WITH' 'NO' 'DATA' | 'WITH' 'DATA' ] ;
```

Functions, procedures, triggers, schemas, extensions
```ebnf
create_function        ::= 'CREATE' [ 'OR' 'REPLACE' ] 'FUNCTION' function_name '(' function_parameters? ')'
                           'RETURNS' function_return
                           function_properties
                           'LANGUAGE' identifier
                           function_body ;

function_parameters    ::= function_parameter { ',' function_parameter } ;
function_parameter     ::= [ 'IN' | 'OUT' | 'INOUT' | 'VARIADIC' ] identifier type_name [ default_clause ] ;
default_clause         ::= 'DEFAULT' expression ;
function_return        ::= type_name | 'TABLE' '(' identifier type_name { ',' identifier type_name } ')' ;
function_properties    ::= { 'IMMUTABLE' | 'STABLE' | 'VOLATILE' | 'PARALLEL' ( 'SAFE' | 'RESTRICTED' | 'UNSAFE' ) | 'STRICT' | 'RETURNS' 'NULL' 'ON' 'NULL' 'INPUT' | 'CALLED' 'ON' 'NULL' 'INPUT' | 'SECURITY' ( 'DEFINER' | 'INVOKER' ) | 'LEAKPROOF' | cost_rows | set_config } ;
cost_rows              ::= 'COST' numeric_literal | 'ROWS' numeric_literal ;
set_config             ::= 'SET' identifier '=' expression ;
function_body          ::= 'AS' dollar_string [ ',' dollar_string ]
                         | 'AS' string_literal [ ',' string_literal ] ;

create_procedure       ::= 'CREATE' [ 'OR' 'REPLACE' ] 'PROCEDURE' function_name '(' function_parameters? ')' [ procedure_properties ] 'LANGUAGE' identifier function_body ;
procedure_properties   ::= { 'TRANSFORM' 'FOR' 'TYPE' '(' type_name { ',' type_name } ')' | 'SECURITY' ( 'DEFINER' | 'INVOKER' ) } ;

create_trigger         ::= 'CREATE' 'TRIGGER' identifier trigger_timing trigger_events 'ON' relation_expr [ 'FOR' 'EACH' ( 'ROW' | 'STATEMENT' ) ] [ 'WHEN' '(' expression ')' ] 'EXECUTE' 'FUNCTION' function_name '(' [ expression_list ] ')' ;
trigger_timing         ::= 'BEFORE' | 'AFTER' | 'INSTEAD' 'OF' ;
trigger_events         ::= 'INSERT' | 'UPDATE' [ 'OF' column_list ] | 'DELETE' | 'TRUNCATE' ;

create_schema          ::= 'CREATE' 'SCHEMA' [ 'IF' 'NOT' 'EXISTS' ] ( identifier | 'AUTHORIZATION' identifier ) ;

create_extension       ::= 'CREATE' 'EXTENSION' [ 'IF' 'NOT' 'EXISTS' ] identifier [ 'WITH' '(' ext_option_list ')' ] ;
ext_option_list        ::= ext_option { ',' ext_option } ;
ext_option             ::= 'SCHEMA' '=' identifier | 'VERSION' '=' string_literal | 'CASCADE' ;
```

Utility and session
```ebnf
copy_statement         ::= 'COPY' ( relation_expr [ '(' column_list ')' ] | '(' select_statement ')' ) 'TO' program_or_file copy_with
                         | 'COPY' relation_expr [ '(' column_list ')' ] 'FROM' program_or_file copy_with ;
program_or_file        ::= string_literal | 'PROGRAM' string_literal | 'STDIN' | 'STDOUT' ;
copy_with              ::= [ 'WITH' '(' copy_option { ',' copy_option } ')' ] ;
copy_option            ::= 'FORMAT' '=' ( 'TEXT' | 'CSV' | 'BINARY' ) | 'DELIMITER' '=' string_literal | 'NULL' '=' string_literal | 'HEADER' '=' boolean_literal | 'QUOTE' '=' string_literal | 'ESCAPE' '=' string_literal | 'FORCE_QUOTE' '=' '(' column_list ')' | 'FORCE_NOT_NULL' '=' '(' column_list ')' | 'ENCODING' '=' string_literal ;

explain_statement      ::= 'EXPLAIN' [ 'ANALYZE' ] [ 'VERBOSE' ] [ '(' explain_option { ',' explain_option } ')' ] ( select_statement | insert_statement | update_statement | delete_statement | merge_statement ) ;
explain_option         ::= identifier [ '=' expression ] ;

vacuum_statement       ::= 'VACUUM' [ '(' vacuum_option_list ')' ] [ relation_expr [ '(' column_list ')' ] ] ;
vacuum_option_list     ::= vacuum_option { ',' vacuum_option } ;
vacuum_option          ::= 'FULL' | 'FREEZE' | 'VERBOSE' | 'ANALYZE' | 'DISABLE_PAGE_SKIPPING' | 'SKIP_LOCKED' | 'INDEX_CLEANUP' '=' ( 'ON' | 'OFF' ) | 'TRUNCATE' '=' ( 'ON' | 'OFF' ) | 'PARALLEL' '=' integer_literal ;

analyze_statement      ::= 'ANALYZE' [ 'VERBOSE' ] [ relation_expr [ '(' column_list ')' ] ] ;

comment_statement      ::= 'COMMENT' 'ON' comment_target 'IS' ( string_literal | 'NULL' ) ;
comment_target         ::= 'TABLE' relation_expr | 'COLUMN' relation_expr '.' identifier | 'INDEX' identifier | 'SCHEMA' identifier | 'FUNCTION' function_name '(' { type_name { ',' type_name } } ')' | 'AGGREGATE' function_name '(' type_name ')' | 'VIEW' relation_expr | 'MATERIALIZED' 'VIEW' relation_expr | 'SEQUENCE' relation_expr | 'TYPE' identifier ;

grant_statement        ::= 'GRANT' privileges 'ON' privilege_target 'TO' grantee_list [ 'WITH' 'GRANT' 'OPTION' ] ;
revoke_statement       ::= 'REVOKE' [ 'GRANT' 'OPTION' 'FOR' ] privileges 'ON' privilege_target 'FROM' grantee_list [ 'CASCADE' | 'RESTRICT' ] ;
privileges             ::= ( 'ALL' [ 'PRIVILEGES' ] ) | privilege_list ;
privilege_list         ::= privilege_item { ',' privilege_item } ;
privilege_item         ::= 'SELECT' | 'INSERT' | 'UPDATE' [ '(' column_list ')' ] | 'DELETE' | 'TRUNCATE' | 'REFERENCES' [ '(' column_list ')' ] | 'TRIGGER' | 'USAGE' | 'CREATE' | 'CONNECT' | 'TEMPORARY' ;
privilege_target       ::= 'TABLE' relation_expr | 'SEQUENCE' relation_expr | 'DATABASE' identifier | 'SCHEMA' identifier | 'FUNCTION' function_name | 'LANGUAGE' identifier | 'ALL' 'TABLES' 'IN' 'SCHEMA' identifier ;
grantee_list           ::= grantee { ',' grantee } ;
grantee                ::= identifier | 'PUBLIC' ;

set_statement          ::= 'SET' ( 'SESSION' | 'LOCAL' )? set_parameter ;
set_parameter          ::= identifier ( '=' | 'TO' ) ( expression | 'DEFAULT' ) | 'TIME' 'ZONE' ( expression | 'LOCAL' | 'DEFAULT' ) ;

transaction_statement  ::= 'BEGIN' [ 'TRANSACTION' ] [ iso_level ] [ access_mode ] [ deferrable_mode ]
                         | 'START' 'TRANSACTION' [ iso_level ] [ access_mode ] [ deferrable_mode ]
                         | 'COMMIT' [ 'WORK' ] [ 'AND' 'NO' 'CHAIN' | 'AND' 'CHAIN' ]
                         | 'ROLLBACK' [ 'WORK' ] [ 'AND' 'NO' 'CHAIN' | 'AND' 'CHAIN' ]
                         | 'SAVEPOINT' identifier
                         | 'RELEASE' 'SAVEPOINT' identifier
                         | 'ROLLBACK' 'TO' [ 'SAVEPOINT' ] identifier ;
iso_level              ::= 'ISOLATION' 'LEVEL' ( 'READ' 'UNCOMMITTED' | 'READ' 'COMMITTED' | 'REPEATABLE' 'READ' | 'SERIALIZABLE' ) ;
access_mode            ::= 'READ' 'ONLY' | 'READ' 'WRITE' ;
deferrable_mode        ::= 'DEFERRABLE' | 'NOT' 'DEFERRABLE' ;
```

Top-level statement list
```ebnf
statement               ::= select_statement
                          | insert_statement
                          | update_statement
                          | delete_statement
                          | merge_statement
                          | create_table | alter_table | drop_table
                          | create_index | drop_index
                          | create_sequence | 'ALTER' 'SEQUENCE' ... | 'DROP' 'SEQUENCE' ...
                          | create_type | 'DROP' 'TYPE' ...
                          | create_view | create_materialized_view | refresh_materialized_view | 'DROP' 'VIEW' ... | 'DROP' 'MATERIALIZED' 'VIEW' ...
                          | create_function | create_procedure | create_trigger | 'DROP' 'FUNCTION' ... | 'DROP' 'PROCEDURE' ... | 'DROP' 'TRIGGER' ...
                          | create_schema | 'DROP' 'SCHEMA' ... | create_extension | 'ALTER' 'EXTENSION' ... | 'DROP' 'EXTENSION' ...
                          | copy_statement | explain_statement | vacuum_statement | analyze_statement
                          | comment_statement | grant_statement | revoke_statement | set_statement | transaction_statement
                          | 'TRUNCATE' [ 'TABLE' ] relation_expr { ',' relation_expr } [ 'CONTINUE' 'IDENTITY' | 'RESTART' 'IDENTITY' ] [ 'CASCADE' | 'RESTRICT' ] ;

sql_script              ::= { statement ';' } ;
```

Dialect highlights and parser considerations
- PostgreSQL supports `DISTINCT ON ( ... )` in `SELECT`.
- LATERAL subqueries and function tables are first-class.
- Arrays: type-suffixed arrays (`int[]`) and `ARRAY[...]` constructors with slicing syntax.
- JSON/JSONB: extensive operators (->, ->>, @>, <@, ? etc.) and functions; operators should be tokenized ahead of precedence resolution.
- UPSERT via `INSERT ... ON CONFLICT ... DO ...` supports index expression targets and partial index predicates (`WHERE` in conflict target).
- Table partitioning and table inheritance coexist; `ONLY table` affects scans in UPDATE/DELETE/TRUNCATE.
- Exclusion constraints with operator classes.
- Identity and generated stored columns.
- Window frames include `GROUPS` and frame exclusions.
- `MERGE` follows SQL:2016 semantics in PG15+ with some PostgreSQL-specific behaviors.

This EBNF is intentionally modular so expression grammar and statement grammar can be reused in other dialect grammars.
