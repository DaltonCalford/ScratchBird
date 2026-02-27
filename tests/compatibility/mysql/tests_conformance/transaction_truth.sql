-- A55-022 MySQL emulation transaction truth script
CREATE TABLE IF NOT EXISTS sb_tx_truth (
  id INT PRIMARY KEY,
  val INT
);
DELETE FROM sb_tx_truth;

START TRANSACTION;
INSERT INTO sb_tx_truth(id, val) VALUES (1, 10);
COMMIT;
SELECT CONCAT('ROW_RESULT|TX-001|',
              IF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 1), 'PASS', 'FAIL'),
              '|commit_visibility');

START TRANSACTION;
INSERT INTO sb_tx_truth(id, val) VALUES (2, 20);
ROLLBACK;
SELECT CONCAT('ROW_RESULT|TX-002|',
              IF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 2), 'FAIL', 'PASS'),
              '|rollback_invisibility');

START TRANSACTION;
INSERT INTO sb_tx_truth(id, val) VALUES (3, 30);
SAVEPOINT sp1;
UPDATE sb_tx_truth SET val = 31 WHERE id = 3;
ROLLBACK TO SAVEPOINT sp1;
COMMIT;
SELECT CONCAT('ROW_RESULT|TX-003|',
              IF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 3 AND val = 30), 'PASS', 'FAIL'),
              '|savepoint_rollback');

SELECT 'ROW_RESULT|TX-004|NA|error_rollback_requires_harness';
SELECT 'ROW_RESULT|TX-005|NA|isolation_requires_multi_session';
SELECT 'ROW_RESULT|TX-006|NA|lock_conflict_requires_multi_session';
