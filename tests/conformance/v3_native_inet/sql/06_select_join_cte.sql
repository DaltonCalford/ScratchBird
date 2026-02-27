CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_sel_y;
DROP TABLE IF EXISTS v3inet.v3_sel_x;

CREATE TABLE v3inet.v3_sel_x (
    id INTEGER PRIMARY KEY,
    grp INTEGER,
    val VARCHAR(16)
);

CREATE TABLE v3inet.v3_sel_y (
    id INTEGER PRIMARY KEY,
    x_id INTEGER,
    score INTEGER
);

INSERT INTO v3inet.v3_sel_x (id, grp, val) VALUES
    (1, 1, 'x1'),
    (2, 1, 'x2'),
    (3, 2, 'x3'),
    (4, 2, 'x4');

INSERT INTO v3inet.v3_sel_y (id, x_id, score) VALUES
    (100, 1, 10),
    (101, 1, 20),
    (102, 2, 30),
    (103, 5, 40);

WITH grp_cte AS (
    SELECT grp, COUNT(*) AS c
    FROM v3inet.v3_sel_x
    GROUP BY grp
)
SELECT 'ASSERT|select|cte_group_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM grp_cte;

SELECT 'ASSERT|select|inner_join_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_sel_x x
INNER JOIN v3inet.v3_sel_y y ON y.x_id = x.id;

SELECT 'ASSERT|select|left_unmatched_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_sel_x x
LEFT JOIN v3inet.v3_sel_y y ON y.x_id = x.id
WHERE y.id IS NULL;

SELECT 'ASSERT|select|right_unmatched_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_sel_x x
RIGHT JOIN v3inet.v3_sel_y y ON y.x_id = x.id
WHERE x.id IS NULL;

SELECT 'ASSERT|select|full_join_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_sel_x x
FULL OUTER JOIN v3inet.v3_sel_y y ON y.x_id = x.id;

SELECT 'ASSERT|select|union_ids|' || CAST(COUNT(*) AS VARCHAR(20))
FROM (
  SELECT id AS idv FROM v3inet.v3_sel_x
  UNION
  SELECT x_id AS idv FROM v3inet.v3_sel_y
) t;

SELECT 'ASSERT|select|intersect_ids|' || CAST(COUNT(*) AS VARCHAR(20))
FROM (
  SELECT id AS idv FROM v3inet.v3_sel_x
  INTERSECT
  SELECT x_id AS idv FROM v3inet.v3_sel_y
) t;

SELECT 'ASSERT|select|except_ids|' || CAST(COUNT(*) AS VARCHAR(20))
FROM (
  SELECT id AS idv FROM v3inet.v3_sel_x
  EXCEPT
  SELECT x_id AS idv FROM v3inet.v3_sel_y
) t;
