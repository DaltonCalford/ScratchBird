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

