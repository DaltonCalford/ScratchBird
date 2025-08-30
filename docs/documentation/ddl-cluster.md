### DDL: Cluster

What it is
- Definitions for clustered deployments (cluster, nodes, services).

Why it matters
- Provides a model for distributed setups and service roles.

How to use it
- Create cluster objects to describe topology; alter/drop during lifecycle.

Parsing for cluster, cluster node, and cluster service is implemented in `src/engine/parser_ddl.cpp`:
- `ast.ddlCluster`, `ast.ddlClusterNode`, `ast.ddlClusterService` each capture action, name, options.
See also
- [Publication & subscription](./ddl-publication-subscription.md)
Example:
```sql
CREATE CLUSTER c1 OPTIONS (...);
CREATE CLUSTER NODE n1 OPTIONS (...);
```

