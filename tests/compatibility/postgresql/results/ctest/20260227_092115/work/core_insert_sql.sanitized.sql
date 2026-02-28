CREATE TABLE IF NOT EXISTS inserttest (col1 int4, col2 int4 NOT NULL, col3 text default 'testing');

insert into inserttest (col2, col3) values (3, DEFAULT);

insert into inserttest (col1, col2, col3) values (DEFAULT, 5, DEFAULT);

insert into inserttest values (DEFAULT, 5, 'test');

select * from inserttest;

select * from inserttest;

select * from inserttest;

insert into inserttest values(30, 50, repeat('x', 10000));

select col1, col2, char_length(col3) from inserttest;

CREATE TABLE large_tuple_test (a int, b text) WITH (fillfactor = 10);

select * from inserttest;

CREATE TABLE IF NOT EXISTS inserttest2 (f1 bigint, f2 text);

CREATE TABLE IF NOT EXISTS inserttest2 (f1 bigint, f2 text);

CREATE TABLE IF NOT EXISTS inserttest3 (f1 text default 'foo', f2 text default 'bar', f3 int);

