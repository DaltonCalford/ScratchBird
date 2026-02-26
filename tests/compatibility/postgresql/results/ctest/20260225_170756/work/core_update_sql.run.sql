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
CREATE TABLE update_test (
    a   INT DEFAULT 10,
    b   INT,
    c   TEXT
);

INSERT INTO update_test VALUES (5, 10, 'foo');

INSERT INTO update_test(b, a) VALUES (15, 10);

SELECT * FROM update_test;

UPDATE update_test SET a = DEFAULT, b = DEFAULT;

SELECT * FROM update_test;

UPDATE update_test AS t SET b = 10 WHERE t.a = 10;

SELECT * FROM update_test;

UPDATE update_test t SET b = t.b + 10 WHERE t.a = 10;

SELECT * FROM update_test;

SELECT * FROM update_test;

INSERT INTO update_test SELECT a,b+1,c FROM update_test;

SELECT * FROM update_test;

SELECT * FROM update_test;

SELECT * FROM update_test;

SELECT * FROM update_test;

SELECT * FROM update_test;

SELECT * FROM update_test;

UPDATE update_test AS t SET b = update_test.b + 10 WHERE t.a = 10;

UPDATE update_test SET c = repeat('x', 10000) WHERE c = 'car';

SELECT a, b, char_length(c) FROM update_test;

SELECT a, b, char_length(c) FROM update_test;

RESET SESSION AUTHORIZATION;

RESET SESSION AUTHORIZATION;

RESET SESSION AUTHORIZATION;

RESET SESSION AUTHORIZATION;

RESET SESSION AUTHORIZATION;

RESET SESSION AUTHORIZATION;

RESET SESSION AUTHORIZATION;

INSERT into list_part1 VALUES ('a', 1);

INSERT into list_default VALUES ('d', 10);

UPDATE list_default set a = 'x' WHERE a = 'd';

CREATE TABLE IF NOT EXISTS utr1 (a int check (a in (1)), q text, b text);

CREATE TABLE IF NOT EXISTS utr2 (a int check (a in (2)), b text);

CREATE TABLE sub_part1(b int, c int8, a numeric);

CREATE TABLE sub_part2(b int, c int8, a numeric);

CREATE TABLE list_part1(a numeric, b int, c int8);

INSERT into list_parted VALUES (2,5,50);

INSERT into list_parted VALUES (3,6,60);

INSERT into sub_parted VALUES (1,1,60);

INSERT into sub_parted VALUES (1,2,10);

UPDATE sub_parted set a = 2 WHERE c = 10;

UPDATE list_parted set b = c + a WHERE a = 2;

UPDATE list_parted set c = 70 WHERE b  = 1;

UPDATE list_parted set b = 1 WHERE c = 70;

UPDATE list_parted set b = 1 WHERE c = 70;

CREATE TABLE non_parted (id int);

INSERT into non_parted VALUES (1), (1), (1), (2), (2), (2), (3), (3), (3);

UPDATE list_parted t1 set a = 2 FROM non_parted t2 WHERE t1.a = t2.id and a = 1;

create operator class custom_opclass for type int4 using hash as
operator 1 = , function 2 dummy_hashint4(int4, int8);

insert into hpart1 values (1, 1);

insert into hpart2 values (2, 5);

insert into hpart4 values (3, 4);

update hash_parted set b = b - 1 where b = 1;

update hash_parted set b = b + 8 where b = 1;

drop operator class custom_opclass using hash;

