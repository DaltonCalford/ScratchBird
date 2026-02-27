CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_delete_case;

CREATE TABLE v3inet.v3_delete_case (
    id INTEGER PRIMARY KEY,
    grp INTEGER,
    payload VARCHAR(32)
);

INSERT INTO v3inet.v3_delete_case (id, grp, payload) VALUES
    (1, 0, 'p1'),
    (2, 1, 'p2'),
    (3, 1, 'p3'),
    (4, 2, 'p4'),
    (5, 2, 'p5'),
    (6, 1, 'p6'),
    (7, 0, 'p7'),
    (8, 2, 'p8'),
    (9, 1, 'p9'),
    (10, 0, 'p10');

DELETE FROM v3inet.v3_delete_case
WHERE grp = 1;

SELECT 'ASSERT|delete|after_delete_where|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_delete_case;

TRUNCATE TABLE v3inet.v3_delete_case;

SELECT 'ASSERT|delete|after_truncate|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_delete_case;

INSERT INTO v3inet.v3_delete_case (id, grp, payload) VALUES
    (1, 9, 'a'),
    (2, 9, 'b'),
    (3, 9, 'c'),
    (4, 9, 'd'),
    (5, 9, 'e');

DELETE FROM v3inet.v3_delete_case
WHERE id BETWEEN 2 AND 4;

SELECT 'ASSERT|delete|after_between_delete|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_delete_case;

SELECT 'ASSERT|delete|remaining_id_sum|' || CAST(SUM(id) AS VARCHAR(20))
FROM v3inet.v3_delete_case;
