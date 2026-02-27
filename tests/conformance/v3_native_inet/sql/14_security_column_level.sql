
DROP VIEW IF EXISTS v3_sec_col_view;
DROP TABLE IF EXISTS v3_sec_col_base;

CREATE ROLE v3_sec_col_user;

CREATE TABLE v3_sec_col_base (
    id INTEGER PRIMARY KEY,
    public_col VARCHAR(32),
    private_col VARCHAR(32)
);

INSERT INTO v3_sec_col_base (id, public_col, private_col) VALUES
    (1, 'p1', 's1');

CREATE VIEW v3_sec_col_view AS
SELECT id, public_col
FROM v3_sec_col_base;

GRANT SELECT ON TABLE v3_sec_col_view TO v3_sec_col_user;

SHOW VIEW v3_sec_col_view;
