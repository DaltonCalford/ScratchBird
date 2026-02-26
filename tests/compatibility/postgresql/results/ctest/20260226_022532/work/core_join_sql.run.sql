CREATE TABLE IF NOT EXISTS onek (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM onek;
INSERT INTO onek VALUES
    (0, 999, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 'ATAAAA', 'ATAAAA', 'AAAA'),
    (1, 998, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 'BBAAAA', 'BBAAAA', 'BBBB'),
    (2, 997, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 'CCAAAA', 'CCAAAA', 'CCCC');

CREATE TABLE IF NOT EXISTS onek2 (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM onek2;
INSERT INTO onek2 SELECT * FROM onek;

CREATE TABLE IF NOT EXISTS tenk1 (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM tenk1;
INSERT INTO tenk1 SELECT * FROM onek;

CREATE TABLE IF NOT EXISTS tenk2 (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM tenk2;
INSERT INTO tenk2 SELECT * FROM tenk1;

CREATE TABLE IF NOT EXISTS person (
    name text, age int4, location text
);
DELETE FROM person;
INSERT INTO person VALUES ('alice', 21, '(0,0)'), ('bob', 35, '(1,1)');

CREATE TABLE IF NOT EXISTS emp (
    name text, age int4, location text, salary int4, manager text
);
DELETE FROM emp;
INSERT INTO emp VALUES ('eve', 40, '(2,2)', 1000, 'alice');

CREATE TABLE IF NOT EXISTS student (
    name text, age int4, location text, gpa float8
);
DELETE FROM student;
INSERT INTO student VALUES ('sam', 19, '(3,3)', 3.8);

CREATE TABLE IF NOT EXISTS stud_emp (
    name text, age int4, location text, salary int4, manager text, gpa float8, percent int4
);
DELETE FROM stud_emp;
INSERT INTO stud_emp VALUES ('pat', 22, '(4,4)', 900, 'eve', 3.6, 50);

CREATE TABLE IF NOT EXISTS int8_tbl (q1 int8, q2 int8);
DELETE FROM int8_tbl;
INSERT INTO int8_tbl VALUES (123, 456), (4567890123456789, 123);
CREATE TABLE J1_TBL (
  i integer,
  j integer,
  t text
);

CREATE TABLE J2_TBL (
  i integer,
  k integer
);

INSERT INTO J1_TBL VALUES (1, 4, 'one');

INSERT INTO J1_TBL VALUES (2, 3, 'two');

INSERT INTO J1_TBL VALUES (3, 2, 'three');

INSERT INTO J1_TBL VALUES (4, 1, 'four');

INSERT INTO J1_TBL VALUES (5, 0, 'five');

INSERT INTO J1_TBL VALUES (6, 6, 'six');

INSERT INTO J1_TBL VALUES (7, 7, 'seven');

INSERT INTO J1_TBL VALUES (8, 8, 'eight');

INSERT INTO J1_TBL VALUES (0, NULL, 'zero');

INSERT INTO J1_TBL VALUES (NULL, NULL, 'null');

INSERT INTO J1_TBL VALUES (NULL, 0, 'zero');

INSERT INTO J2_TBL VALUES (1, -1);

INSERT INTO J2_TBL VALUES (2, 2);

INSERT INTO J2_TBL VALUES (3, -3);

INSERT INTO J2_TBL VALUES (2, 4);

INSERT INTO J2_TBL VALUES (5, -5);

INSERT INTO J2_TBL VALUES (5, -5);

INSERT INTO J2_TBL VALUES (0, NULL);

INSERT INTO J2_TBL VALUES (NULL, NULL);

INSERT INTO J2_TBL VALUES (NULL, 0);

CREATE TABLE IF NOT EXISTS onerow();

analyze onerow;

SELECT *
  FROM J1_TBL AS tx;

SELECT *
  FROM J1_TBL tx;

SELECT *
  FROM J1_TBL AS t1 (a, b, c);

SELECT *
  FROM J1_TBL t1 (a, b, c);

SELECT *
  FROM J1_TBL t1 (a, b, c), J2_TBL t2 (d, e);

SELECT t1.a, t2.e
  FROM J1_TBL t1 (a, b, c), J2_TBL t2 (d, e)
  WHERE t1.a = t2.d;

SELECT *
  FROM J1_TBL CROSS JOIN J2_TBL;

SELECT i, k, t
  FROM J1_TBL CROSS JOIN J2_TBL;

SELECT t1.i, k, t
  FROM J1_TBL t1 CROSS JOIN J2_TBL t2;

SELECT *
  FROM J1_TBL CROSS JOIN J2_TBL a CROSS JOIN J2_TBL b;

SELECT *
  FROM J1_TBL INNER JOIN J2_TBL USING (i);

SELECT *
  FROM J1_TBL JOIN J2_TBL USING (i);

SELECT * FROM J1_TBL JOIN J2_TBL USING (i) WHERE J1_TBL.t = 'one';

SELECT *
  FROM J1_TBL JOIN J2_TBL ON (J1_TBL.i = J2_TBL.i);

SELECT *
  FROM J1_TBL JOIN J2_TBL ON (J1_TBL.i = J2_TBL.k);

SELECT *
  FROM J1_TBL JOIN J2_TBL ON (J1_TBL.i <= J2_TBL.k);

SELECT *
  FROM J1_TBL LEFT OUTER JOIN J2_TBL USING (i)
  ORDER BY i, k, t;

SELECT *
  FROM J1_TBL LEFT JOIN J2_TBL USING (i)
  ORDER BY i, k, t;

SELECT *
  FROM J1_TBL RIGHT OUTER JOIN J2_TBL USING (i);

SELECT *
  FROM J1_TBL RIGHT JOIN J2_TBL USING (i);

SELECT *
  FROM J1_TBL FULL OUTER JOIN J2_TBL USING (i)
  ORDER BY i, k, t;

SELECT *
  FROM J1_TBL FULL JOIN J2_TBL USING (i)
  ORDER BY i, k, t;

SELECT *
  FROM J1_TBL LEFT JOIN J2_TBL USING (i) WHERE (k = 1);

SELECT *
  FROM J1_TBL LEFT JOIN J2_TBL USING (i) WHERE (i = 1);

CREATE TABLE t1 (name TEXT, n INTEGER);

CREATE TABLE t2 (name TEXT, n INTEGER);

CREATE TABLE t3 (name TEXT, n INTEGER);

INSERT INTO t1 VALUES ( 'bb', 11 );

INSERT INTO t2 VALUES ( 'bb', 12 );

INSERT INTO t2 VALUES ( 'cc', 22 );

INSERT INTO t2 VALUES ( 'ee', 42 );

INSERT INTO t3 VALUES ( 'bb', 13 );

INSERT INTO t3 VALUES ( 'cc', 23 );

INSERT INTO t3 VALUES ( 'dd', 33 );

SELECT * FROM t1 FULL JOIN t2 USING (name) FULL JOIN t3 USING (name);

CREATE TABLE IF NOT EXISTS x (x1 int, x2 int);

insert into x values (1,11);

insert into x values (2,22);

insert into x values (3,null);

insert into x values (4,44);

insert into x values (5,null);

CREATE TABLE IF NOT EXISTS y (y1 int, y2 int);

insert into y values (1,111);

insert into y values (2,222);

insert into y values (3,333);

insert into y values (4,null);

select * from x;

select * from y;

select * from x left join y on (x1 = y1 and x2 is not null);

select * from x left join y on (x1 = y1 and y2 is not null);

