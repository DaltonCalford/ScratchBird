# Cassandra CQL Specification

**Status:** Draft (Beta)

## 1. Purpose

Define Cassandra Query Language (CQL) for wide-column stores, including DDL,
DML, and TTL semantics. This spec is for dialect emulation and parser design.

## 2. Core Concepts

- **Keyspace**: Namespace with replication settings.
- **Table**: Column family with partition key and clustering columns.
- **Primary key**: `PRIMARY KEY ((partition_key), clustering...)`.
- **TTL**: Per-row or per-column expiration.

## 3. EBNF

### 3.1 DDL

```ebnf
create_keyspace = "CREATE" "KEYSPACE" if_not_exists keyspace
                  "WITH" replication_clause
                  [ "AND" durable_writes_clause ] ;

replication_clause = "REPLICATION" "=" map_literal ;

durable_writes_clause = "DURABLE_WRITES" "=" boolean ;

create_table   = "CREATE" "TABLE" if_not_exists table_name
                 "(" column_def { "," column_def } ","
                 "PRIMARY" "KEY" "(" primary_key ")" ")"
                 [ "WITH" table_options ] ;

column_def     = identifier type [ "STATIC" ] ;
primary_key    = partition_key [ "," clustering_key_list ] ;
partition_key  = identifier | "(" identifier { "," identifier } ")" ;
clustering_key_list = identifier { "," identifier } ;

table_options  = option { "AND" option } ;
option         = identifier "=" value ;
```

### 3.2 DML

```ebnf
insert_stmt    = "INSERT" "INTO" table_name "(" column_list ")"
                 "VALUES" "(" value_list ")"
                 [ "USING" using_clause ] ;

using_clause   = using_item { "AND" using_item } ;
using_item     = "TTL" number | "TIMESTAMP" number ;

update_stmt    = "UPDATE" table_name
                 [ "USING" using_clause ]
                 "SET" set_item { "," set_item }
                 "WHERE" where_clause ;

set_item       = column_name "=" expr
              | column_name "=" column_name plus_minus expr ;

delete_stmt    = "DELETE" [ column_list ] "FROM" table_name
                 "WHERE" where_clause ;

select_stmt    = "SELECT" select_list "FROM" table_name
                 [ "WHERE" where_clause ]
                 [ "ORDER" "BY" order_clause ]
                 [ "LIMIT" number ] ;
```

### 3.3 WHERE

```ebnf
where_clause   = condition { "AND" condition } ;
condition      = column_name op value
              | column_name "IN" "(" value_list ")" ;

op            = "=" | "<" | "<=" | ">" | ">=" ;
```

## 4. Semantics

- Partition key is mandatory in WHERE for efficient lookup.
- Clustering columns define order within a partition.
- `USING TTL` sets expiration for inserted/updated data.
- `USING TIMESTAMP` applies a write timestamp for conflict resolution.

## 5. Usage Guidance

**When to use CQL**
- Write-heavy workloads with predictable access patterns.
- Partitioned data with time or tenant keys.

**Avoid when**
- Ad-hoc queries requiring full table scans or joins.

## 6. Examples

**Create table**
```sql
CREATE TABLE events (
  tenant_id text,
  ts timestamp,
  event_id uuid,
  payload text,
  PRIMARY KEY ((tenant_id), ts, event_id)
) WITH CLUSTERING ORDER BY (ts DESC);
```

**Insert with TTL**
```sql
INSERT INTO events (tenant_id, ts, event_id, payload)
VALUES ('t1', toTimestamp(now()), uuid(), 'x')
USING TTL 3600;
```

