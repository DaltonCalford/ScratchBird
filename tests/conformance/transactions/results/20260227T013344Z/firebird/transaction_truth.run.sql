CREATE DATABASE 'a55_fb_tx_20260227013344_20393';
CONNECT 'a55_fb_tx_20260227013344_20393';
-- A55-022 Firebird emulation transaction truth script
RECREATE TABLE sb_tx_truth (
  id INTEGER NOT NULL PRIMARY KEY,
  val INTEGER
);
COMMIT;

SET AUTODDL OFF;

SET TRANSACTION;
INSERT INTO sb_tx_truth(id, val) VALUES (1, 10);
COMMIT;
SELECT 'ROW_RESULT|TX-001|' ||
       IIF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 1), 'PASS', 'FAIL') ||
       '|commit_visibility';

SET TRANSACTION;
INSERT INTO sb_tx_truth(id, val) VALUES (2, 20);
ROLLBACK;
SELECT 'ROW_RESULT|TX-002|' ||
       IIF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 2), 'FAIL', 'PASS') ||
       '|rollback_invisibility';

SET TRANSACTION;
INSERT INTO sb_tx_truth(id, val) VALUES (3, 30);
SAVEPOINT sp1;
UPDATE sb_tx_truth SET val = 31 WHERE id = 3;
ROLLBACK TO SAVEPOINT sp1;
COMMIT;
SELECT 'ROW_RESULT|TX-003|' ||
       IIF(EXISTS(SELECT 1 FROM sb_tx_truth WHERE id = 3 AND val = 30), 'PASS', 'FAIL') ||
       '|savepoint_rollback';

SELECT 'ROW_RESULT|TX-004|NA|error_rollback_requires_harness';
SELECT 'ROW_RESULT|TX-005|NA|isolation_requires_multi_session';
SELECT 'ROW_RESULT|TX-006|NA|lock_conflict_requires_multi_session';
