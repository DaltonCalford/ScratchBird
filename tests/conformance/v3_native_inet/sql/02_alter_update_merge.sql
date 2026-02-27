CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_alter_case;

CREATE TABLE v3inet.v3_alter_case (
    id INTEGER PRIMARY KEY,
    state VARCHAR(32),
    qty INTEGER
);

INSERT INTO v3inet.v3_alter_case (id, state, qty) VALUES (1, 'old', 10);
INSERT INTO v3inet.v3_alter_case (id, state, qty) VALUES (2, 'old', 20);

UPDATE v3inet.v3_alter_case
SET qty = qty + 5,
    state = 'updated'
WHERE id = 2;

ALTER TABLE v3inet.v3_alter_case ADD COLUMN note VARCHAR(64);
INSERT INTO v3inet.v3_alter_case (id, state, qty, note) VALUES (3, 'new', 30, 'added');
