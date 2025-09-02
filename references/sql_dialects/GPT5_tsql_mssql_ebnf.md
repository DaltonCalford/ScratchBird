### T-SQL (Microsoft SQL Server) — EBNF Report

Version scope: SQL Server 2019–2022 features. Focus on T‑SQL constructs: TOP/OFFSET, APPLY, OUTPUT clause, MERGE, table hints, proprietary functions, and procedural T‑SQL (BEGIN...END, DECLARE, TRY/CATCH).

EBNF index:
- Tokens and lexical items
- Expressions and precedence
- Queries (SELECT, set ops, CTE, TOP/OFFSET, APPLY, table hints)
- DML (INSERT with OUTPUT, UPDATE, DELETE, MERGE)
- DDL (tables, computed columns, sequences, indexes, schemas, views)
- Procedural T‑SQL (variables, control flow, error handling)
- Utilities (EXEC, sp_executesql, SET, GRANT/REVOKE)

Tokens and lexical items
```ebnf
identifier             ::= unquoted_identifier | quoted_identifier | bracket_identifier ;
unquoted_identifier    ::= letter { letter | digit | '_' | '$' } ;
quoted_identifier      ::= '"' { qchar } '"' ;
bracket_identifier     ::= '[' { bchar } ']' ;

letter                 ::= 'A'..'Z' | 'a'..'z' | '_' ;
digit                  ::= '0'..'9' ;
qchar                  ::= any_character_except('"') | '""' ;
bchar                  ::= any_character_except(']') | ']]' ;

string_literal         ::= '\'' { schar } '\'' ;
schar                  ::= any_character_except('\'', '\\') | '\\' any_character ;

numeric_literal        ::= integer_literal | decimal_literal | float_literal ;
integer_literal        ::= digit { digit } ;
decimal_literal        ::= digit { digit } '.' { digit } | '.' digit { digit } ;
float_literal          ::= ( integer_literal | decimal_literal ) ( 'E' | 'e' ) [ '+' | '-' ] integer_literal ;

binary_literal         ::= '0x' { hex_digit } ;
hex_digit              ::= digit | 'A'..'F' | 'a'..'f' ;

boolean_literal        ::= 'TRUE' | 'FALSE' ; -- treated as bit 1/0 in some contexts
null_literal           ::= 'NULL' ;

parameter_reference    ::= '@' identifier ;
```

Expressions and precedence
```ebnf
expression             ::= or_expression ;
or_expression          ::= and_expression { 'OR' and_expression } ;
and_expression         ::= not_expression { 'AND' not_expression } ;
not_expression         ::= [ 'NOT' ] comparison_expression ;

comparison_expression  ::= add_expression comparison_tail? ;
comparison_tail        ::= comparison_operator add_expression
                         | [ 'NOT' ] 'BETWEEN' add_expression 'AND' add_expression
                         | [ 'NOT' ] 'IN' '(' in_list_or_subquery ')'
                         | [ 'NOT' ] 'LIKE' add_expression [ 'ESCAPE' add_expression ]
                         | 'IS' [ 'NOT' ] ( 'NULL' | 'TRUE' | 'FALSE' | 'UNKNOWN' ) ;

comparison_operator    ::= '=' | '!=' | '<>' | '<' | '<=' | '>' | '>=' ;

in_list_or_subquery    ::= expression { ',' expression } | select_statement ;

add_expression         ::= mult_expression { ( '+' | '-' | '|' ) mult_expression } ; -- bitwise or included
mult_expression        ::= unary_expression { ( '*' | '/' | '%' | '&' | '^' ) unary_expression } ;

unary_expression       ::= [ '+' | '-' | '~' ] postfix_expression
                         | 'EXISTS' '(' select_statement ')'
                         | 'CASE' case_operand? when_clauses [ 'ELSE' expression ] 'END' ;

case_operand           ::= expression ;
when_clauses           ::= 'WHEN' expression 'THEN' expression { 'WHEN' expression 'THEN' expression } ;

postfix_expression     ::= primary_expression postfix_suffix* ;
postfix_suffix         ::= field_access | type_cast | collate_suffix ;
field_access           ::= '.' identifier ;
type_cast              ::= 'CAST' '(' expression 'AS' type_name ')' | 'CONVERT' '(' type_name ',' expression ')' ;
collate_suffix         ::= 'COLLATE' identifier ;

primary_expression     ::= literal | parameter_reference | column_reference | function_call | select_subquery | '(' expression ')' ;
literal                ::= numeric_literal | string_literal | binary_literal | boolean_literal | null_literal ;
column_reference       ::= identifier [ '.' identifier ] [ '.' identifier ] [ '.' identifier ] ;

function_call          ::= function_name '(' [ function_args ] ')' window_clause? ;
function_name          ::= identifier [ '.' identifier ] ;
function_args          ::= [ 'DISTINCT' ] expression { ',' expression } ;

window_clause          ::= 'OVER' ( window_name | window_spec ) ;
window_name            ::= identifier ;
window_spec            ::= '(' [ 'PARTITION' 'BY' expression_list ] [ 'ORDER' 'BY' order_list ] [ rows_range ] ')' ;
expression_list        ::= expression { ',' expression } ;
order_list             ::= order_item { ',' order_item } ;
order_item             ::= expression [ 'ASC' | 'DESC' ] ;
rows_range             ::= 'ROWS' frame_range | 'RANGE' frame_range ;
frame_range            ::= frame_start | 'BETWEEN' frame_start 'AND' frame_end ;
frame_start            ::= 'UNBOUNDED' 'PRECEDING' | expression 'PRECEDING' | 'CURRENT' 'ROW' ;
frame_end              ::= 'CURRENT' 'ROW' | expression 'FOLLOWING' | 'UNBOUNDED' 'FOLLOWING' ;

type_name              ::= identifier [ '(' expression_list ')' ] ;
```

Queries (SELECT, CTE, APPLY, hints)
```ebnf
select_statement       ::= [ with_clause ] select_core { set_operation select_core } [ order_by_clause ] [ offset_fetch ] [ table_hint_clause ] ;

with_clause            ::= 'WITH' [ 'RECURSIVE' ]? cte_definition { ',' cte_definition } ; -- SQL Server uses WITH; recursion via UNION ALL
cte_definition         ::= identifier [ '(' identifier { ',' identifier } ')' ] 'AS' '(' select_core { set_operation select_core } ')' ;

set_operation          ::= 'UNION' [ 'ALL' | 'DISTINCT' ] | 'INTERSECT' | 'EXCEPT' ;

select_core            ::= 'SELECT' [ 'ALL' | 'DISTINCT' ] [ top_clause ] select_list from_clause? where_clause? group_by_clause? having_clause? ;
top_clause             ::= 'TOP' ( integer_literal | parameter_reference | '(' expression ')' ) [ 'PERCENT' ] [ 'WITH' 'TIES' ] ;
select_list            ::= select_item { ',' select_item } ;
select_item            ::= expression [ alias ] | '*' | qualified_star ;
qualified_star         ::= identifier '.' '*' | identifier '.' identifier '.' '*' ;
alias                  ::= [ 'AS' ] identifier ;

from_clause            ::= 'FROM' table_reference { ',' table_reference } ;
table_reference        ::= table_factor joined_tail* ;
table_factor           ::= relation_expr [ alias ] [ table_sample ]
                         | '(' select_statement ')' [ alias ]
                         | '(' table_reference ')' ;
relation_expr          ::= identifier [ '.' identifier ] [ '.' identifier ] ;
table_sample           ::= 'TABLESAMPLE' 'SYSTEM' '(' integer_literal 'PERCENT' ')' ;

joined_tail            ::= join_op table_factor [ join_condition ] ;
join_op                ::= 'JOIN' | 'INNER' 'JOIN' | 'LEFT' [ 'OUTER' ] 'JOIN' | 'RIGHT' [ 'OUTER' ] 'JOIN' | 'FULL' [ 'OUTER' ] 'JOIN' | 'CROSS' 'JOIN' | 'APPLY' | 'CROSS' 'APPLY' | 'OUTER' 'APPLY' ;
join_condition         ::= 'ON' expression ;

where_clause           ::= 'WHERE' expression ;
group_by_clause        ::= 'GROUP' 'BY' expression_list [ 'WITH' 'ROLLUP' ] ;
having_clause          ::= 'HAVING' expression ;

order_by_clause        ::= 'ORDER' 'BY' order_list ;
offset_fetch           ::= 'OFFSET' integer_literal 'ROWS' [ 'FETCH' ( 'FIRST' | 'NEXT' ) integer_literal 'ROWS' ( 'ONLY' | 'WITH' 'TIES' ) ] ;

table_hint_clause      ::= 'OPTION' '(' hint_list ')' ;
hint_list              ::= hint { ',' hint } ;
hint                   ::= identifier [ '(' expression_list ')' ] ;
```

DML (OUTPUT, MERGE)
```ebnf
insert_statement       ::= 'INSERT' 'INTO' relation_expr [ '(' column_list ')' ] insert_source [ output_clause ] ;
insert_source          ::= 'DEFAULT' 'VALUES' | 'VALUES' values_list | select_statement ;
values_list            ::= '(' value_list ')' { ',' '(' value_list ')' } ;
value_list             ::= expression { ',' expression } ;
column_list            ::= identifier { ',' identifier } ;

output_clause          ::= 'OUTPUT' output_list [ 'INTO' relation_expr '(' column_list ')' ] ;
output_list            ::= expression { ',' expression } ; -- inserted.*, deleted.* supported via virtual tables

update_statement       ::= 'UPDATE' relation_expr 'SET' set_list [ from_clause ] [ where_clause ] [ output_clause ] ;
set_list               ::= set_item { ',' set_item } ;
set_item               ::= identifier '=' expression ;

delete_statement       ::= 'DELETE' 'FROM' relation_expr [ from_clause ] [ where_clause ] [ output_clause ] ;

merge_statement        ::= 'MERGE' 'INTO' relation_expr [ alias ] 'USING' ( relation_expr | '(' select_statement ')' [ alias ] ) 'ON' expression merge_when { merge_when } [ output_clause ] ;
merge_when             ::= 'WHEN' 'MATCHED' [ 'AND' expression ] 'THEN' merge_action | 'WHEN' 'NOT' 'MATCHED' [ 'AND' expression ] 'THEN' merge_action ;
merge_action           ::= 'UPDATE' 'SET' set_list [ where_clause ] | 'DELETE' | 'INSERT' '(' column_list ')' 'VALUES' '(' value_list ')' [ where_clause ] ;
```

DDL (tables, indexes, sequences, views)
```ebnf
create_table           ::= 'CREATE' 'TABLE' relation_expr '(' table_element { ',' table_element } ')' ;
table_element          ::= column_definition | table_constraint ;
column_definition      ::= identifier type_name column_constraints? ;
column_constraints     ::= { 'NULL' | 'NOT' 'NULL' | 'DEFAULT' expression | 'IDENTITY' '(' integer_literal ',' integer_literal ')' | computed_column } ;
computed_column        ::= 'AS' '(' expression ')' [ 'PERSISTED' ] ;

table_constraint       ::= [ 'CONSTRAINT' identifier ] ( 'PRIMARY' 'KEY' '(' column_list ')' | 'UNIQUE' '(' column_list ')' | 'CHECK' '(' expression ')' | foreign_key ) ;
foreign_key            ::= 'FOREIGN' 'KEY' '(' column_list ')' 'REFERENCES' relation_expr '(' column_list ')' [ ref_actions ] ;
ref_actions            ::= [ 'ON' 'DELETE' ref_action ] [ 'ON' 'UPDATE' ref_action ] ;
ref_action             ::= 'NO' 'ACTION' | 'CASCADE' | 'SET' 'NULL' | 'SET' 'DEFAULT' ;

create_sequence        ::= 'CREATE' 'SEQUENCE' relation_expr sequence_options? ;
sequence_options       ::= { 'AS' type_name | 'START' 'WITH' integer_literal | 'INCREMENT' 'BY' integer_literal | 'MINVALUE' integer_literal | 'MAXVALUE' integer_literal | 'CYCLE' | 'CACHE' integer_literal | 'NO' 'CYCLE' } ;

create_index           ::= 'CREATE' [ 'UNIQUE' ] 'INDEX' identifier 'ON' relation_expr '(' index_col_list ')' ;
index_col_list         ::= index_col { ',' index_col } ;
index_col              ::= identifier [ 'ASC' | 'DESC' ] ;

create_view            ::= 'CREATE' [ 'OR' 'ALTER' ] 'VIEW' relation_expr [ '(' column_list ')' ] 'AS' select_statement ;
```

Procedural T‑SQL
```ebnf
batch                  ::= { batch_statement } ;
batch_statement        ::= sql_statement ';' | block_statement ;

block_statement        ::= 'BEGIN' statement_list 'END' ;
statement_list         ::= { statement_item } ;
statement_item         ::= sql_statement ';' | declare_statement ';' | set_statement ';' | control_statement ';' | try_catch_block ;

declare_statement      ::= 'DECLARE' declare_item { ',' declare_item } ;
declare_item           ::= parameter_reference type_name [ '=' expression ] ;

set_statement          ::= 'SET' parameter_reference '=' expression | 'SET' 'NOCOUNT' 'ON' | 'SET' 'NOCOUNT' 'OFF' ;

control_statement      ::= if_statement | while_statement | cursor_statement | return_statement | print_statement ;
if_statement           ::= 'IF' expression block_statement [ 'ELSE' block_statement ] ;
while_statement        ::= 'WHILE' expression block_statement ;
cursor_statement       ::= 'DECLARE' identifier 'CURSOR' 'FOR' select_statement | 'OPEN' identifier | 'FETCH' identifier 'INTO' column_list | 'CLOSE' identifier | 'DEALLOCATE' identifier ;
return_statement       ::= 'RETURN' [ expression ] ;
print_statement        ::= 'PRINT' expression ;

try_catch_block        ::= 'BEGIN' 'TRY' statement_list 'END' 'TRY' 'BEGIN' 'CATCH' statement_list 'END' 'CATCH' ;

sql_statement          ::= select_statement | insert_statement | update_statement | delete_statement | merge_statement | create_table | create_index | create_sequence | create_view ;
```

Utilities
```ebnf
exec_statement         ::= 'EXEC' ( function_name [ expression_list ] | 'sp_executesql' string_literal [ ',' exec_params ] ) ;
exec_params            ::= parameter_reference '=' expression { ',' parameter_reference '=' expression } ;
grant_statement        ::= 'GRANT' ... ; revoke_statement ::= 'REVOKE' ... ;
```

Highlights
- T‑SQL `TOP` and `OFFSET/FETCH` coexist; `APPLY` supports table-valued functions.
- `OUTPUT` clause exposes `inserted`/`deleted` pseudo-tables across DML.
- Procedural T‑SQL provides `BEGIN/END`, variables with `@`, and robust `TRY/CATCH`.

