### DDL: Cluster

Parsing for cluster, cluster node, and cluster service is implemented in `src/engine/parser_ddl.cpp`:
- `ast.ddlCluster`, `ast.ddlClusterNode`, `ast.ddlClusterService` each capture action, name, options.

Example:
```sql
CREATE CLUSTER c1 OPTIONS (...);
CREATE CLUSTER NODE n1 OPTIONS (...);
```

