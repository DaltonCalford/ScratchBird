# Cypher / openCypher Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the Cypher graph query language for property graphs. This specification
covers core query grammar, semantics, and usage guidance for dialect emulation.

## 2. Core Concepts

- **Property graph**: Nodes with labels, relationships with types, both with
  properties (key/value pairs).
- **Pattern matching**: MATCH clauses bind variables to nodes/relationships.
- **Optional matching**: OPTIONAL MATCH yields nulls when no match exists.
- **Path variables**: Bind entire paths for downstream use.

## 3. EBNF

### 3.1 Query Structure

```ebnf
query         = single_query { union_clause } ;
union_clause  = "UNION" [ "ALL" ] single_query ;

single_query  = { clause } return_clause ;

clause        = match_clause
              | optional_match_clause
              | create_clause
              | merge_clause
              | set_clause
              | remove_clause
              | delete_clause
              | with_clause
              | unwind_clause
              | call_clause ;

match_clause  = "MATCH" pattern [ where_clause ] ;
optional_match_clause = "OPTIONAL" "MATCH" pattern [ where_clause ] ;

create_clause = "CREATE" pattern ;
merge_clause  = "MERGE" pattern [ merge_action ] ;
merge_action  = ("ON" "MATCH" set_clause) | ("ON" "CREATE" set_clause) ;

set_clause    = "SET" set_item { "," set_item } ;
set_item      = variable "." property_name "=" expr
              | variable ":" label_name ;

remove_clause = "REMOVE" remove_item { "," remove_item } ;
remove_item   = variable ":" label_name
              | variable "." property_name ;

delete_clause = ("DELETE" | "DETACH" "DELETE") delete_item { "," delete_item } ;
delete_item   = variable ;

with_clause   = "WITH" return_items [ where_clause ] ;
return_clause = "RETURN" return_items [ order_by ] [ skip ] [ limit ] ;

unwind_clause = "UNWIND" expr "AS" variable ;

call_clause   = "CALL" procedure_call [ "YIELD" yield_items ] ;

where_clause  = "WHERE" expr ;
order_by      = "ORDER" "BY" order_item { "," order_item } ;
order_item    = expr [ "ASC" | "DESC" ] ;
skip          = "SKIP" expr ;
limit         = "LIMIT" expr ;
```

### 3.2 Pattern Grammar

```ebnf
pattern       = pattern_part { "," pattern_part } ;
pattern_part  = path_pattern | node_pattern ;
path_pattern  = node_pattern { rel_pattern node_pattern } ;

node_pattern  = "(" [ variable ] [ label_list ] [ properties ] ")" ;
label_list    = ":" label_name { ":" label_name } ;

rel_pattern   = left_arrow? "-" relationship_detail "-" right_arrow? ;
left_arrow    = "<" ;
right_arrow   = ">" ;

relationship_detail = "[" [ variable ] [ rel_type_list ]
                       [ properties ] [ rel_range ] "]" ;
rel_type_list = ":" rel_type { "|" ":" rel_type } ;
rel_range     = "*" [ range_min ] [ ".." [ range_max ] ] ;
range_min     = number ;
range_max     = number ;
```

### 3.3 Expressions

```ebnf
return_items  = return_item { "," return_item } ;
return_item   = expr [ "AS" alias ] ;

expr          = literal
              | variable
              | variable "." property_name
              | func_call
              | case_expr
              | "(" expr ")"
              | expr binary_op expr
              | unary_op expr ;

binary_op     = "=" | "<>" | "<" | "<=" | ">" | ">="
              | "AND" | "OR" | "+" | "-" | "*" | "/" | "%"
              | "IN" | "CONTAINS" | "STARTS" "WITH" | "ENDS" "WITH" ;

unary_op      = "NOT" | "-" ;
```

## 4. Semantics

- **MATCH** performs pattern matching and binds variables per match.
- **OPTIONAL MATCH** is a left-outer match; unbound variables become null.
- **CREATE** inserts new nodes/relationships exactly as specified.
- **MERGE** matches or creates; `ON MATCH`/`ON CREATE` control updates.
- **DELETE** removes entities; `DETACH DELETE` removes attached relationships.
- **WITH** projects intermediate results and resets scope.
- **UNWIND** expands a list into rows.

## 5. Usage Guidance

**When to use Cypher**
- Graph-heavy data modeling with pattern matching.
- Traversals that benefit from expressive path syntax.

**Avoid when**
- Workloads are purely relational without graph relationships.

## 6. Examples

**Match and return**
```cypher
MATCH (u:User)-[:FOLLOWS]->(v:User)
WHERE u.active = true
RETURN u.name, v.name
ORDER BY v.name
LIMIT 20;
```

**Optional match**
```cypher
MATCH (u:User)
OPTIONAL MATCH (u)-[:HAS_PROFILE]->(p:Profile)
RETURN u.id, p.bio;
```

**Merge with updates**
```cypher
MERGE (u:User {email: "a@example.com"})
ON CREATE SET u.created_at = datetime()
ON MATCH SET u.last_seen = datetime()
RETURN u;
```

