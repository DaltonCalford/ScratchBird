# MongoDB Query Language (MQL) Specification

**Status:** Draft (Beta)

## 1. Purpose

Define a complete specification for MongoDB Query Language (MQL), including
query filters, projections, updates, and the aggregation pipeline. This
spec is intended for dialect emulation and parser design.

## 2. Core Concepts

- **Document**: JSON-like map of key -> value.
- **Field path**: Dot-delimited path into nested documents, e.g. `user.profile.id`.
- **Array semantics**: Predicates on arrays match if any element satisfies the
  predicate unless an explicit array operator is used.
- **Types**: Scalar (null, boolean, number, string), object, array, binary.

## 3. EBNF (Core JSON)

```ebnf
json          = value ;
value         = object | array | string | number | "true" | "false" | "null" ;
object        = "{" [ member { "," member } ] "}" ;
member        = string ":" value ;
array         = "[" [ value { "," value } ] "]" ;
string        = '"' { char } '"' ;
number        = [ "-" ] int [ frac ] [ exp ] ;
int           = "0" | ( digit1-9 { digit } ) ;
frac          = "." { digit } ;
exp           = ("e" | "E") ["+" | "-"] { digit } ;
```

## 4. Query Filter Grammar

### 4.1 EBNF

```ebnf
filter        = object ;
filter_member = field_path ":" predicate
              | logical_op ":" array ;

field_path    = string ;

predicate     = value
              | object ;

logical_op    = "$and" | "$or" | "$nor" | "$not" ;

comparison_op = "$eq" | "$ne" | "$gt" | "$gte" | "$lt" | "$lte"
              | "$in" | "$nin" ;

array_op      = "$all" | "$elemMatch" | "$size" ;

element_op    = "$exists" | "$type" ;

eval_op       = "$regex" | "$mod" ;

geo_op        = "$geoWithin" | "$geoIntersects" | "$near" | "$nearSphere" ;

predicate     = value
              | "{" predicate_op { "," predicate_op } "}" ;

predicate_op  = comparison_op ":" value
              | array_op ":" value
              | element_op ":" value
              | eval_op ":" value
              | geo_op ":" value ;
```

### 4.2 Semantics

- `field_path: value` is equality.
- `field_path: { $op: value }` applies the operator to the field.
- `logical_op` takes an array of filter objects.
- `regex` uses implementation-defined regex engine; flags are passed via
  `$options` or inline.

## 5. Projection Grammar

```ebnf
projection    = "{" proj_item { "," proj_item } "}" ;
proj_item     = field_path ":" ( "0" | "1" | expression ) ;
expression    = object ;
```

Notes:
- `1` includes field, `0` excludes field.
- Expressions are allowed in aggregation (`$project`) not in simple find.

## 6. Update Grammar

```ebnf
update_doc    = "{" update_op { "," update_op } "}" ;
update_op     = update_op_name ":" object ;
update_op_name = "$set" | "$unset" | "$inc" | "$mul" | "$min" | "$max"
               | "$rename" | "$currentDate" | "$setOnInsert"
               | "$push" | "$pull" | "$addToSet" | "$pop" | "$bit" ;
```

Semantics:
- Update documents apply per matched document.
- Array updates use positional `$` operator or array filters (if supported).

## 7. Aggregation Pipeline Grammar

```ebnf
pipeline      = "[" stage { "," stage } "]" ;
stage         = "{" stage_op ":" stage_body "}" ;

stage_op      = "$match" | "$project" | "$group" | "$sort" | "$limit" | "$skip"
              | "$unwind" | "$lookup" | "$facet" | "$count" | "$addFields"
              | "$set" | "$unset" | "$replaceRoot" | "$replaceWith"
              | "$bucket" | "$bucketAuto" | "$sample" | "$geoNear"
              | "$graphLookup" | "$merge" | "$out" ;
```

Stage semantics are standard MongoDB pipeline behaviors. `$match` uses the
filter grammar, `$project` uses projection expressions, and `$group` defines
accumulators such as `$sum`, `$avg`, `$min`, `$max`, `$push`, `$addToSet`.

## 8. Usage Guidance

**When to use MQL**
- Document-centric applications with nested JSON and heterogeneous fields.
- Query patterns rely on flexible schemas and array matching.

**Avoid when**
- Workloads require complex relational joins or strict schema constraints.

## 9. Examples

**Find with comparison and array match**
```json
{ "status": "active", "tags": { "$all": ["red", "sale"] } }
```

**Update with modifiers**
```json
{ "$set": { "status": "archived" }, "$currentDate": { "updated_at": true } }
```

**Aggregation pipeline**
```json
[
  { "$match": { "status": "active" } },
  { "$group": { "_id": "$category", "count": { "$sum": 1 } } },
  { "$sort": { "count": -1 } }
]
```

