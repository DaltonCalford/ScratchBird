CREATE TABLE vncr_940f11_t1 (
    a INTEGER PRIMARY KEY,
    t TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    c VARCHAR(10) DEFAULT 'hello',
    i INTEGER
);
-- Frozen one-time native-v3 translation of the donor AUTO_INCREMENT task:
-- seed the implicit ids inside the static corpus so runtime execution stays
-- translation-free while preserving the donor-visible result contract.
INSERT INTO vncr_940f11_t1 (a, t, c, i)
VALUES ((SELECT COALESCE(MAX(a), 0) + 1 FROM vncr_940f11_t1), DEFAULT, DEFAULT, DEFAULT);
INSERT INTO vncr_940f11_t1 (a, t, c, i)
VALUES ((SELECT COALESCE(MAX(a), 0) + 1 FROM vncr_940f11_t1), DEFAULT, DEFAULT, DEFAULT);
INSERT INTO vncr_940f11_t1 (a, c, i) VALUES (4, 'a', 5);
SELECT 'ASSERT|mysql_insert_defaults_identity|rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_940f11_t1;
SELECT 'ASSERT|mysql_insert_defaults_identity|hello_rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_940f11_t1 WHERE c = 'hello';
SELECT 'ASSERT|mysql_insert_defaults_identity|explicit_id|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_940f11_t1 WHERE a = 4;
DROP TABLE vncr_940f11_t1;
