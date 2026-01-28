# Lucene Query Syntax Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the Lucene query syntax used by Lucene, Elasticsearch `query_string`,
and related search tools. This spec includes the grammar, operators, and
usage guidance for emulation.

## 2. Core Concepts

- **Terms** and **phrases**
- **Fielded queries** (`field:value`)
- **Boolean operators** (`AND`, `OR`, `NOT`)
- **Wildcards** and **fuzzy matching**
- **Range queries**

## 3. EBNF

```ebnf
query         = or_expr ;

or_expr       = and_expr { ("OR" | "||") and_expr } ;
and_expr      = not_expr { ("AND" | "&&") not_expr } ;
not_expr      = [ ("NOT" | "!") ] primary ;

primary       = clause | "(" query ")" ;
clause        = [ field ":" ] term_or_group ;

term_or_group = phrase | term | range | wildcard | fuzzy | proximity ;

term          = word ;
phrase        = "\"" { phrase_char } "\"" ;

range         = range_inclusive | range_exclusive ;
range_inclusive = "[" value "TO" value "]" ;
range_exclusive = "{" value "TO" value "}" ;

wildcard      = word { "*" | "?" } ;
fuzzy         = word "~" [ number ] ;
proximity     = phrase "~" number ;

field         = identifier ;
word          = identifier | number ;
value         = word | "*" ;
```

## 4. Operators and Modifiers

- **Boolean**: `AND`, `OR`, `NOT` or `&&`, `||`, `!`
- **Required/Prohibited**: `+term` required, `-term` prohibited
- **Phrase**: `"quick brown"`
- **Wildcard**: `foo*`, `b?ar`
- **Fuzzy**: `roam~0.8`
- **Proximity**: `"foo bar"~5`
- **Boost**: `term^2.0`

## 5. Semantics

- Terms are analyzed by the configured analyzer for the field.
- Phrase queries match adjacent terms in order unless proximity is used.
- Wildcards operate on the analyzed term index (implementation-dependent).
- Range queries include or exclude boundaries based on bracket type.

## 6. Usage Guidance

**When to use Lucene syntax**
- Ad-hoc search text boxes
- Power users needing flexible search expressions

**Avoid when**
- Structured filtering or complex aggregations are required (use DSL instead).

## 7. Examples

**Basic fielded search**
```
title:"database systems" AND year:[2020 TO 2024]
```

**Wildcard and fuzzy**
```
name:john* OR name:jon~
```

**Required and prohibited**
```
+status:active -type:legacy
```

