CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_tx_case;

CREATE TABLE v3inet.v3_tx_case (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(32),
    qty INTEGER
);

BEGIN;
INSERT INTO v3inet.v3_tx_case (id, payload, qty) VALUES (1, 'first', 10);
SAVEPOINT sp_one;
INSERT INTO v3inet.v3_tx_case (id, payload, qty) VALUES (2, 'rollback_me', 20);
ROLLBACK TO SAVEPOINT sp_one;
INSERT INTO v3inet.v3_tx_case (id, payload, qty) VALUES (3, 'kept', 30);
COMMIT;

SELECT 'ASSERT|tx|rows_after_commit|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_tx_case;

SELECT 'ASSERT|tx|sum_ids_after_commit|' || CAST(SUM(id) AS VARCHAR(20))
FROM v3inet.v3_tx_case;

BEGIN;
UPDATE v3inet.v3_tx_case SET qty = 999 WHERE id = 1;
ROLLBACK;

SELECT 'ASSERT|tx|qty_after_rollback|' || CAST(qty AS VARCHAR(20))
FROM v3inet.v3_tx_case
WHERE id = 1;
