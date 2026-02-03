# Sequences

**Last Updated:** 2026-02-03

---

Sequences generate unique numeric values. You can use them directly or through
identity/serial columns.

---

## Create a Sequence

```sql
CREATE SEQUENCE order_seq START 1 INCREMENT 1;
```

## Use a Sequence

```sql
SELECT NEXTVAL('order_seq');
SELECT CURRVAL('order_seq');
```

## SERIAL / IDENTITY

```sql
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    total DECIMAL(10,2)
);

CREATE TABLE invoices (
    id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    total DECIMAL(10,2)
);
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
