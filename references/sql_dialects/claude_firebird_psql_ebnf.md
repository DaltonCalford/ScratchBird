### Firebird SQL and PSQL — EBNF Report

Version scope: Firebird 3.0–5.0 features. Focus on SQL DML/DDL, PSQL (procedural SQL) blocks, exceptions, and triggers.

EBNF index:
- Tokens and lexical specifics
- Expressions and operators
- Queries (SELECT, set ops, CTE, windowing)
- DML (INSERT/UPDATE/DELETE/MERGE) with RETURNING
- DDL (domains, tables, generators/sequences, collations, computed columns)
- PSQL blocks (BEGIN..END), variables, cursors, exceptions
- Triggers, procedures, functions
- Utilities (SET, GRANT/REVOKE, COMMENT)

Tokens and lexical specifics
```ebnf
identifier           ::= unquoted_identifier | quoted_identifier ;
unquoted_identifier  ::= letter { letter | digit | '_' | '$' } ;
quoted_identifier    ::= '"' { qchar } '"' ;

letter               ::= 'A'..'Z' | 'a'..'z' | '_' ;
digit                ::= '0'..'9' ;
qchar                ::= any_character_except('"') | '""' ;

string_literal       ::= '\'' { schar } '\'' ;
schar                ::= any_character_except('\'', '\\') | '\\' any_character ;

numeric_literal      ::= integer_literal | decimal_literal | float_literal ;
integer_literal      ::= digit { digit } ;
decimal_literal      ::= digit { digit } '.' { digit } ;
float_literal        ::= ( integer_literal | decimal_literal ) ( 'E' | 'e' ) [ '+' | '-' ] integer_literal ;

boolean_literal      ::= 'TRUE' | 'FALSE' ;
null_literal         ::= 'NULL' ;

parameter_reference  ::= ':' identifier | '?' ; -- PSQL uses :name
```

Expressions and operators
```ebnf
expression            ::= or_expression ;
or_expression         ::= and_expression { 'OR' and_expression } ;
and_expression        ::= not_expression { 'AND' not_expression } ;
not_expression        ::= [ 'NOT' ] comparison_expression ;

comparison_expression ::= add_expression comparison_tail? ;
comparison_tail       ::= comparison_operator add_expression
                        | [ 'NOT' ] 'BETWEEN' add_expression 'AND' add_expression
                        | [ 'NOT' ] 'IN' '(' in_list_or_subquery ')'
                        | [ 'NOT' ] 'LIKE' add_expression [ 'ESCAPE' add_expression ]
                        | 'IS' [ 'NOT' ] ( 'NULL' | 'TRUE' | 'FALSE' | 'UNKNOWN' ) ;

comparison_operator   ::= '=' | '<>' | '!=' | '<' | '<=' | '>' | '>=' ;

in_list_or_subquery   ::= expression { ',' expression } | select_statement ;

add_expression        ::= mult_expression { ( '+' | '-' ) mult_expression } ;
mult_expression       ::= unary_expression { ( '*' | '/' ) unary_expression } ;
unary_expression      ::= [ '+' | '-' ] postfix_expression | 'EXISTS' '(' select_statement ')' | 'CASE' case_operand? when_clauses [ 'ELSE' expression ] 'END' ;
case_operand          ::= expression ;
when_clauses          ::= 'WHEN' expression 'THEN' expression { 'WHEN' expression 'THEN' expression } ;

postfix_expression    ::= primary_expression postfix_suffix* ;
postfix_suffix        ::= field_access | type_cast | collate_suffix ;
field_access          ::= '.' identifier ;
type_cast             ::= 'CAST' '(' expression 'AS' type_name ')' ;
collate_suffix        ::= 'COLLATE' identifier ;

primary_expression    ::= literal | parameter_reference | column_reference | function_call | select_subquery | '(' expression ')' ;
literal               ::= numeric_literal | string_literal | boolean_literal | null_literal ;
column_reference      ::= identifier [ '.' identifier ] [ '.' identifier ] ;

function_call         ::= function_name '(' [ expression_list ] ')' window_clause? ;
function_name         ::= identifier [ '.' identifier ] ;
expression_list       ::= expression { ',' expression } ;

window_clause         ::= 'OVER' '(' [ 'PARTITION' 'BY' expression_list ] [ 'ORDER' 'BY' order_list ] [ frame_clause ] ')' ;
order_list            ::= order_item { ',' order_item } ;
order_item            ::= expression [ 'ASC' | 'DESC' ] ;
frame_clause          ::= 'ROWS' frame_range ;
frame_range           ::= frame_start | 'BETWEEN' frame_start 'AND' frame_end ;
frame_start           ::= 'UNBOUNDED' 'PRECEDING' | expression 'PRECEDING' | 'CURRENT' 'ROW' ;
frame_end             ::= 'CURRENT' 'ROW' | expression 'FOLLOWING' | 'UNBOUNDED' 'FOLLOWING' ;

type_name             ::= identifier [ '(' expression_list ')' ] ;
```

Queries (SELECT, set operations, CTE)
```ebnf
select_statement      ::= [ with_clause ] select_core { set_operation select_core } [ order_by_clause ] [ rows_clause ] [ for_update_clause ] ;

with_clause           ::= 'WITH' [ 'RECURSIVE' ] cte_definition { ',' cte_definition } ;
cte_definition        ::= identifier [ '(' identifier { ',' identifier } ')' ] 'AS' '(' select_core { set_operation select_core } ')' ;

set_operation         ::= 'UNION' [ 'ALL' | 'DISTINCT' ] | 'INTERSECT' | 'EXCEPT' ;

select_core           ::= 'SELECT' [ 'ALL' | 'DISTINCT' ] select_list from_clause? where_clause? group_by_clause? having_clause? ;
select_list           ::= select_item { ',' select_item } ;
select_item           ::= expression [ alias ] | '*' | qualified_star ;
qualified_star        ::= identifier '.' '*' | identifier '.' identifier '.' '*' ;
alias                 ::= [ 'AS' ] identifier ;

from_clause           ::= 'FROM' table_reference { ',' table_reference } ;
table_reference       ::= table_factor joined_tail* ;
table_factor          ::= relation_expr [ alias ] | '(' select_statement ')' [ alias ] | '(' table_reference ')' ;
relation_expr         ::= identifier [ '.' identifier ] [ '.' identifier ] ;
joined_tail           ::= join_op table_factor [ join_condition ] ;
join_op               ::= 'JOIN' | 'INNER' 'JOIN' | 'LEFT' [ 'OUTER' ] 'JOIN' | 'RIGHT' [ 'OUTER' ] 'JOIN' | 'FULL' [ 'OUTER' ] 'JOIN' | 'CROSS' 'JOIN' ;
join_condition        ::= 'ON' expression | 'USING' '(' identifier { ',' identifier } ')' ;

where_clause          ::= 'WHERE' expression ;
group_by_clause       ::= 'GROUP' 'BY' expression_list ;
having_clause         ::= 'HAVING' expression ;

order_by_clause       ::= 'ORDER' 'BY' order_list ;
rows_clause           ::= 'ROWS' integer_literal ; -- FIRST N, SKIP N also supported
for_update_clause     ::= 'FOR' 'UPDATE' ;
```

DML (RETURNING, MERGE)
```ebnf
insert_statement      ::= 'INSERT' 'INTO' relation_expr [ '(' column_list ')' ] insert_source [ returning_clause ] ;
insert_source         ::= 'VALUES' values_list | select_statement ;
values_list           ::= '(' value_list ')' { ',' '(' value_list ')' } ;
value_list            ::= expression { ',' expression } ;
column_list           ::= identifier { ',' identifier } ;
returning_clause      ::= 'RETURNING' return_list ;
return_list           ::= '*' | expression_list ;

update_statement      ::= 'UPDATE' relation_expr 'SET' set_list [ where_clause ] [ returning_clause ] ;
set_list              ::= set_item { ',' set_item } ;
set_item              ::= identifier '=' expression ;

delete_statement      ::= 'DELETE' 'FROM' relation_expr [ where_clause ] [ returning_clause ] ;

merge_statement       ::= 'MERGE' 'INTO' relation_expr 'USING' table_reference 'ON' expression merge_when { merge_when } [ returning_clause ] ;
merge_when            ::= 'WHEN' 'MATCHED' 'THEN' merge_action | 'WHEN' 'NOT' 'MATCHED' 'THEN' merge_action ;
merge_action          ::= 'UPDATE' 'SET' set_list [ where_clause ] | 'INSERT' '(' column_list ')' 'VALUES' '(' value_list ')' | 'DELETE' ;
```

DDL (domains, sequences/generators, computed columns)
```ebnf
create_domain         ::= 'CREATE' 'DOMAIN' identifier 'AS' type_name domain_constraints? ;
domain_constraints    ::= { 'DEFAULT' expression | 'CHECK' '(' expression ')' | 'NOT' 'NULL' | 'NULL' } ;

create_table          ::= 'CREATE' 'TABLE' relation_expr '(' table_element { ',' table_element } ')' ;
table_element         ::= column_definition | table_constraint ;
column_definition     ::= identifier type_name column_constraints? ;
column_constraints    ::= { 'NOT' 'NULL' | 'NULL' | 'DEFAULT' expression | 'CHECK' '(' expression ')' | computed_column } ;
computed_column       ::= 'COMPUTED' 'BY' '(' expression ')' ;

table_constraint      ::= [ 'CONSTRAINT' identifier ] ( 'PRIMARY' 'KEY' '(' column_list ')' | 'UNIQUE' '(' column_list ')' | 'CHECK' '(' expression ')' | foreign_key ) ;
foreign_key           ::= 'FOREIGN' 'KEY' '(' column_list ')' 'REFERENCES' relation_expr [ '(' column_list ')' ] [ ref_actions ] ;
ref_actions           ::= [ 'ON' 'DELETE' ref_action ] [ 'ON' 'UPDATE' ref_action ] ;
ref_action            ::= 'CASCADE' | 'SET' 'NULL' | 'SET' 'DEFAULT' | 'NO' 'ACTION' ;

create_sequence       ::= 'CREATE' 'SEQUENCE' identifier sequence_opts? ;
sequence_opts         ::= { 'START' 'WITH' integer_literal | 'INCREMENT' [ 'BY' ] integer_literal | 'MINVALUE' integer_literal | 'MAXVALUE' integer_literal | 'CACHE' integer_literal | 'CYCLE' | 'NOCYCLE' } ;

drop_table            ::= 'DROP' 'TABLE' relation_expr ;
drop_sequence         ::= 'DROP' 'SEQUENCE' identifier ;
```

PSQL blocks (procedural SQL)
```ebnf
psql_block            ::= 'BEGIN' psql_statements 'END' ;
psql_statements       ::= { psql_statement ';' } ;
psql_statement        ::= assignment | if_stmt | while_stmt | for_select_stmt | exception_stmt | execute_stmt | cursor_stmt | suspend_stmt | return_stmt | sql_statement ;

variable_decl         ::= 'DECLARE' identifier type_name [ default_clause ] ;
assignment            ::= identifier ':=' expression ;
if_stmt               ::= 'IF' expression 'THEN' psql_statements [ 'ELSE' psql_statements ] 'END' 'IF' ;
while_stmt            ::= 'WHILE' expression 'DO' psql_statements 'END' 'WHILE' ;
for_select_stmt       ::= 'FOR' identifier 'AS' 'CURSOR' 'FOR' select_statement 'DO' psql_statements 'END' 'FOR' ;
exception_stmt        ::= 'EXCEPTION' identifier [ ',' string_literal ] ;
execute_stmt          ::= 'EXECUTE' 'STATEMENT' expression [ 'INTO' column_list ] ;
cursor_stmt           ::= 'OPEN' identifier | 'CLOSE' identifier | 'FETCH' identifier 'INTO' column_list ;
suspend_stmt          ::= 'SUSPEND' ;
return_stmt           ::= 'RETURN' ;
sql_statement         ::= insert_statement | update_statement | delete_statement | merge_statement | select_statement ;
```

Triggers, procedures, functions
```ebnf
create_trigger        ::= 'CREATE' 'TRIGGER' identifier trigger_position trigger_event 'ON' relation_expr 'AS' psql_block ;
trigger_position      ::= 'BEFORE' | 'AFTER' ;
trigger_event         ::= 'INSERT' | 'UPDATE' | 'DELETE' ;

create_procedure      ::= 'CREATE' 'PROCEDURE' identifier '(' param_list? ')' [ 'RETURNS' '(' param_list ')' ] 'AS' declarations? psql_block ;
create_function       ::= 'CREATE' 'FUNCTION' identifier '(' param_list? ')' 'RETURNS' type_name 'AS' declarations? psql_block ;
param_list            ::= param_decl { ',' param_decl } ;
param_decl            ::= identifier type_name ;
declarations          ::= { variable_decl ';' } ;
```

Utilities
```ebnf
comment_statement     ::= 'COMMENT' 'ON' ( 'TABLE' relation_expr | 'COLUMN' relation_expr '.' identifier ) 'IS' ( string_literal | 'NULL' ) ;
grant_statement       ::= 'GRANT' ... ; revoke_statement ::= 'REVOKE' ... ;
set_statement         ::= 'SET' identifier '=' expression ;
```

Highlights
- Firebird PSQL has explicit `BEGIN...END` blocks, variable declarations, and exception handling distinct from SQL/PSM.
- `RETURNING` is widely supported on DML.
- Computed columns via `COMPUTED BY` on tables; domains for reusable types.
- Sequences (generators) exist; `NEXT VALUE FOR` usage in expressions is permitted.

