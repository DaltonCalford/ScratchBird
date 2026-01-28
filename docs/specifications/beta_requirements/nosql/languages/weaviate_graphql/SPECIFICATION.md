# Weaviate GraphQL Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the GraphQL-based query language used by Weaviate for vector and
hybrid search. This spec covers GraphQL query shape, filters, and search
operators.

## 2. Core Concepts

- **Classes**: Schema entities (similar to collections).
- **Properties**: Scalar fields returned in results.
- **nearVector / nearText**: Vector and hybrid search operators.
- **Filters**: Boolean filters with `where` clause.

## 3. EBNF (GraphQL Subset)

```ebnf
graphql_query = "{" root_field "}" ;

root_field    = "Get" "{" class_query { class_query } "}" ;

class_query   = class_name [ args ] "{" field_list "}" ;

args          = "(" arg { "," arg } ")" ;
arg           = identifier ":" value ;

field_list    = field { field } ;
field         = identifier | object_field ;
object_field  = identifier "{" field_list "}" ;
```

## 4. Search Arguments

Common arguments for class queries:

- `nearVector: { vector: [...], certainty: 0.8 }`
- `nearText: { concepts: ["cat"], certainty: 0.7 }`
- `where: { path: ["field"], operator: Equal, valueString: "x" }`
- `limit: 10`
- `offset: 0`

## 5. Filter Grammar (where)

```ebnf
where_clause  = "{" "path" ":" array ","
                 "operator" ":" operator ","
                 value_field
                 [ "," operands ] "}" ;

operator      = "Equal" | "NotEqual" | "GreaterThan" | "GreaterThanEqual"
              | "LessThan" | "LessThanEqual" | "And" | "Or" ;

value_field   = "valueString" ":" string
              | "valueNumber" ":" number
              | "valueBoolean" ":" boolean
              | "valueDate" ":" string ;

operands      = "operands" ":" "[" where_clause { "," where_clause } "]" ;
```

## 6. Semantics

- `Get` retrieves objects matching filters and vector criteria.
- `nearVector` and `nearText` perform similarity search; `certainty` controls
  threshold.
- `where` filter applies structured constraints.

## 7. Usage Guidance

**When to use Weaviate GraphQL**
- Vector search with GraphQL-style selection sets.
- Hybrid search with text + vector ranking.

**Avoid when**
- Large analytical queries or complex joins are required.

## 8. Examples

**Vector search query**
```graphql
{
  Get {
    Article(
      nearText: { concepts: ["database"] },
      limit: 5
    ) {
      title
      _additional { certainty }
    }
  }
}
```

**Filtered query**
```graphql
{
  Get {
    Article(
      where: { path: ["status"], operator: Equal, valueString: "active" },
      limit: 10
    ) {
      title
      status
    }
  }
}
```

