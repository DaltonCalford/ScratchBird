CREATE TABLE analyze_test (
    id INT PRIMARY KEY,
    value INT,
    note TEXT
);

INSERT INTO analyze_test (id, value, note) VALUES
    (1, 10, 'a'),
    (2, 20, 'b'),
    (3, 30, 'c');

ANALYZE analyze_test;
ANALYZE VERBOSE analyze_test;
ANALYZE analyze_test (value);
ANALYZE analyze_test COLUMN note;
ANALYZE analyze_test SAMPLE 0.5;

DROP TABLE analyze_test;
