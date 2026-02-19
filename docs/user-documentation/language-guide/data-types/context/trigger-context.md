# Trigger Context
Last modified: 2026-02-19

Back links:
- [Context README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Context Variables](context-variables.md)

Trigger context surfaces in runtime:
- row-level old/new value access via `OLD.<column>` and `NEW.<column>` semantics
- statement-level transition table context via statement trigger context accessors

Usage example:
~~~sql
IF (NEW.status IS DISTINCT FROM OLD.status) THEN
  INSERT INTO audit_table(order_id, old_status, new_status, changed_at)
  VALUES (OLD.order_id, OLD.status, NEW.status, CURRENT_TIMESTAMP);
END IF;
~~~
