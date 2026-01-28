# HBase Shell and Filter Language Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the HBase shell command language and filter expressions. This spec
covers command grammar, filter syntax, and usage guidance for emulation.

## 2. Core Concepts

- **Namespace** and **table** management.
- **Column families** and qualifiers.
- **Scan filters** for server-side filtering.

## 3. EBNF (Shell Commands)

```ebnf
command       = create_cmd | put_cmd | get_cmd | scan_cmd | delete_cmd
              | disable_cmd | enable_cmd | drop_cmd | describe_cmd ;

create_cmd    = "create" table_name "," family_def { "," family_def } ;
family_def    = "{" family_option { "," family_option } "}" ;
family_option = identifier "=>" value ;

put_cmd       = "put" table_name "," row_key "," column "," value
                [ "," timestamp ] ;

get_cmd       = "get" table_name "," row_key [ "," get_options ] ;
scan_cmd      = "scan" table_name [ "," scan_options ] ;

delete_cmd    = "delete" table_name "," row_key "," column [ "," timestamp ] ;

disable_cmd   = "disable" table_name ;
enable_cmd    = "enable" table_name ;
drop_cmd      = "drop" table_name ;
describe_cmd  = "describe" table_name ;
```

## 4. Filter Language

```ebnf
filter_expr   = filter | filter_list ;
filter_list   = "{" filter { "," filter } "}" ;

filter        = single_filter | composite_filter ;

single_filter = "PrefixFilter" "(" string ")"
              | "ColumnPrefixFilter" "(" string ")"
              | "QualifierFilter" "(" compare_op "," comparator ")"
              | "ValueFilter" "(" compare_op "," comparator ")"
              | "RowFilter" "(" compare_op "," comparator ")" ;

composite_filter = "FilterList" "(" operator "," filter_list ")" ;
operator      = "MUST_PASS_ALL" | "MUST_PASS_ONE" ;

compare_op    = "=", "<", "<=", ">", ">=" | "!=" ;
comparator    = "BinaryComparator" "(" string ")"
              | "RegexStringComparator" "(" string ")" ;
```

## 5. Semantics

- Shell commands map to HBase admin or data APIs.
- `scan` supports filters for row/column/value pruning.
- Filters may be combined with AND/OR semantics via `FilterList`.

## 6. Usage Guidance

**When to use HBase shell**
- Operational admin tasks and quick data checks.

**Avoid when**
- Complex query requirements beyond simple scans and filters.

## 7. Examples

**Create table**
```
create 'users', {NAME => 'profile'}, {NAME => 'metrics'}
```

**Scan with filter**
```
scan 'users', { FILTER => "PrefixFilter('user_')" }
```

