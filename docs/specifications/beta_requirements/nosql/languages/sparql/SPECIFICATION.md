# SPARQL Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the SPARQL query language for RDF graphs, including query forms,
triple patterns, and solution modifiers. This spec is intended for dialect
emulation and parser design.

## 2. Core Concepts

- **RDF graph**: Set of triples (subject, predicate, object).
- **Variables**: Prefixed with `?` or `$`.
- **Triple pattern**: Subject/predicate/object where each can be a variable.
- **Graph pattern**: Combination of triple patterns with OPTIONAL, UNION, etc.

## 3. EBNF

### 3.1 Query Form

```ebnf
query         = prologue ( select_query | construct_query | ask_query | describe_query ) ;

prologue      = { base_decl | prefix_decl } ;
base_decl     = "BASE" iri_ref ;
prefix_decl   = "PREFIX" prefix_name ":" iri_ref ;

select_query  = "SELECT" [ "DISTINCT" | "REDUCED" ] var_list
                where_clause [ solution_modifiers ] ;

construct_query = "CONSTRUCT" construct_template
                  where_clause [ solution_modifiers ] ;

ask_query     = "ASK" where_clause ;

describe_query = "DESCRIBE" ( var_or_iri { var_or_iri } | "*" )
                 where_clause? [ solution_modifiers ] ;

var_list      = "*" | var { var } ;
var_or_iri    = var | iri_ref ;
```

### 3.2 WHERE and Graph Patterns

```ebnf
where_clause  = "WHERE" group_graph_pattern ;

group_graph_pattern = "{" group_pattern { "." group_pattern } "}" ;

group_pattern = triple_block
              | optional_block
              | union_block
              | graph_block
              | filter
              | bind ;

triple_block  = triple_pattern { "." triple_pattern } ;

triple_pattern = subject predicate object ;
subject       = var | iri_ref | blank_node ;
predicate     = var | iri_ref ;
object        = var | iri_ref | literal | blank_node ;

optional_block = "OPTIONAL" group_graph_pattern ;
union_block    = group_graph_pattern "UNION" group_graph_pattern ;
graph_block    = "GRAPH" var_or_iri group_graph_pattern ;

filter       = "FILTER" expr ;
bind         = "BIND" "(" expr "AS" var ")" ;
```

### 3.3 Expressions and Modifiers

```ebnf
solution_modifiers = [ order_clause ] [ limit_offset ] ;

order_clause  = "ORDER" "BY" order_item { order_item } ;
order_item    = ( "ASC" | "DESC" ) "(" expr ")" | expr ;

limit_offset  = [ "LIMIT" number ] [ "OFFSET" number ] ;

expr          = literal | var | iri_ref | func_call
              | "(" expr ")"
              | expr binary_op expr
              | unary_op expr ;

binary_op     = "=" | "!=" | "<" | "<=" | ">" | ">="
              | "&&" | "||" | "+" | "-" | "*" | "/" ;

unary_op      = "!" | "-" ;
```

## 4. Semantics

- **Basic Graph Pattern (BGP)**: Conjunction of triple patterns; results are
  joined by shared variables.
- **OPTIONAL**: Left-outer join; missing matches yield unbound variables.
- **UNION**: Union of solutions from each branch.
- **FILTER**: Removes solutions where predicate is false or error.
- **GRAPH**: Match against a named graph.

## 5. Usage Guidance

**When to use SPARQL**
- RDF datasets with semantic web standards.
- Triple-store style queries and graph pattern matching.

**Avoid when**
- Property graph workloads (Cypher/Gremlin are more suitable).

## 6. Examples

**Select with filter**
```sparql
PREFIX foaf: <http://xmlns.com/foaf/0.1/>
SELECT ?name
WHERE {
  ?p a foaf:Person .
  ?p foaf:name ?name .
  FILTER(?name != "Alice")
}
ORDER BY ?name
LIMIT 10
```

**Optional + union**
```sparql
SELECT ?s ?label
WHERE {
  { ?s rdfs:label ?label }
  UNION
  { ?s skos:prefLabel ?label }
}
```

