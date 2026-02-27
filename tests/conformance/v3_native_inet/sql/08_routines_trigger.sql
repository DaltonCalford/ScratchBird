CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_rt_table;

CREATE TABLE v3inet.v3_rt_table (
    id INTEGER PRIMARY KEY,
    val INTEGER,
    note VARCHAR(32)
);

SET TERM ^;

CREATE OR ALTER TRIGGER trg_v3inet_rt_seed FOR v3inet.v3_rt_table ACTIVE BEFORE INSERT AS
BEGIN
    IF (NEW.note IS NULL) THEN NEW.note = 'trg_seed';
END^

CREATE FUNCTION v3inet_rt_fn() RETURNS INTEGER AS
BEGIN
    RETURN 2;
END^

CREATE PROCEDURE v3inet_rt_proc AS
BEGIN
    INSERT INTO v3inet.v3_rt_table (id, val, note) VALUES (1, 10, NULL);
END^

SET TERM ;^

EXECUTE PROCEDURE v3inet_rt_proc;

UPDATE v3inet.v3_rt_table
SET val = val * v3inet_rt_fn()
WHERE id = 1;

SELECT 'ASSERT|routine|rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_rt_table;

SELECT 'ASSERT|routine|trigger_seeded|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_rt_table
WHERE note = 'trg_seed';

SELECT 'ASSERT|routine|fn_value|' || CAST(v3inet_rt_fn() AS VARCHAR(20));

SELECT 'ASSERT|routine|updated_val|' || CAST(val AS VARCHAR(20))
FROM v3inet.v3_rt_table
WHERE id = 1;
