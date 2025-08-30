### DDL: Publication and Subscription

What it is
- Logical replication definitions (publications) and subscribers.

Why it matters
- Enables data dissemination across systems; supports decoupled architectures.

How to use it
- Create publications for source tables; create subscriptions to consume.

Parsing for CREATE/ALTER/DROP PUBLICATION and SUBSCRIPTION captured in `ast.ddlPublication` and `ast.ddlSubscription` (name, options, action).
See also
- [Policies (RLS)](./ddl-policies-rls.md) · [Cluster](./ddl-cluster.md)
Example:
```sql
CREATE PUBLICATION pub1 OPTIONS (tables 'public.t');
CREATE SUBSCRIPTION sub1 OPTIONS (publication 'pub1');
```

