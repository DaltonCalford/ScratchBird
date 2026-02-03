# Indexes, Views, Sequences

**Last Updated:** 2026-02-03

---

## Indexes

```sql
CREATE INDEX idx_users_email ON app.users (email);
CREATE INDEX idx_orders_total ON app.orders USING BTREE (total);
```

Common index methods:
Btree, Hash, GiST, GIN, SP-GiST, BRIN, R-tree, Bitmap, LSM, HNSW, IVF,
Columnstore, Full‑text, Inverted, Zone Map.

## Views

```sql
CREATE VIEW app.active_users AS
SELECT * FROM app.users WHERE active = TRUE;
```

## Sequences

```sql
CREATE SEQUENCE app.order_seq START 1;
SELECT NEXTVAL('app.order_seq');
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
