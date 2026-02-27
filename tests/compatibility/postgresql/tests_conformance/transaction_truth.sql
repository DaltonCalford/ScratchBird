-- A55-022 PostgreSQL emulation transaction truth script
CREATE TABLE IF NOT EXISTS sb_tx_truth (
  id INTEGER PRIMARY KEY,
  val INTEGER
);
DELETE FROM sb_tx_truth;

BEGIN;
INSERT INTO sb_tx_truth(id, val) VALUES (1, 10);
COMMIT;
SELECT 'ROW_RESULT|TX-001|PASS|commit_visibility'
FROM sb_tx_truth
WHERE id = 1;
SELECT 'ROW_RESULT|TX-001|FAIL|commit_visibility'
WHERE NOT EXISTS (SELECT 1 FROM sb_tx_truth WHERE id = 1);

BEGIN;
INSERT INTO sb_tx_truth(id, val) VALUES (2, 20);
ROLLBACK;
SELECT 'ROW_RESULT|TX-002|PASS|rollback_invisibility'
WHERE NOT EXISTS (SELECT 1 FROM sb_tx_truth WHERE id = 2);
SELECT 'ROW_RESULT|TX-002|FAIL|rollback_invisibility'
FROM sb_tx_truth
WHERE id = 2;

BEGIN;
INSERT INTO sb_tx_truth(id, val) VALUES (3, 30);
SAVEPOINT sp1;
UPDATE sb_tx_truth SET val = 31 WHERE id = 3;
ROLLBACK TO SAVEPOINT sp1;
COMMIT;
SELECT 'ROW_RESULT|TX-003|PASS|savepoint_rollback'
FROM sb_tx_truth
WHERE id = 3 AND val = 30;
SELECT 'ROW_RESULT|TX-003|FAIL|savepoint_rollback'
WHERE NOT EXISTS (SELECT 1 FROM sb_tx_truth WHERE id = 3 AND val = 30);

SELECT 'ROW_RESULT|TX-004|NA|error_rollback_requires_harness';
SELECT 'ROW_RESULT|TX-005|NA|isolation_requires_multi_session';
SELECT 'ROW_RESULT|TX-006|NA|lock_conflict_requires_multi_session';
