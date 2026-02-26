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

select * from inserttesta;

select * from inserttestb;

CREATE TABLE IF NOT EXISTS inserttest2 (f1 bigint, f2 text);

insert into inserttesta (f1[1]) values (1);

insert into inserttesta (f1[1], f1[2]) values (1, 2);

insert into inserttestb (f1.if1) values (1);

insert into inserttestb (f1.if1, f1.if2) values (1, '{foo}');

set role regress_insert_other_user;

insert into key_desc values (1, 1);

reset role;

set role regress_insert_other_user;

insert into key_desc values (1, 1);

insert into key_desc values (2, 1);

reset role;

drop role regress_insert_other_user;

insert into brtrigpartcon values (1, 'hi there');

insert into brtrigpartcon1 values (1, 'hi there');

CREATE TABLE IF NOT EXISTS inserttest3 (f1 text default 'foo', f2 text default 'bar', f3 int);

create role regress_coldesc_role;

set role regress_coldesc_role;

with result as (insert into brtrigpartcon values (1, 'hi there') returning 1)
  insert into inserttest3 (f3) select * from result;

reset role;

drop role regress_coldesc_role;

CREATE TABLE IF NOT EXISTS donothingbrtrig_test1 (b text, a int);

CREATE TABLE IF NOT EXISTS donothingbrtrig_test2 (c text, b text, a int);

insert into donothingbrtrig_test values (1, 'foo'), (2, 'bar');

insert into returningwrtest values (1) returning returningwrtest;

CREATE TABLE IF NOT EXISTS returningwrtest2 (b text, c int, a int);

insert into returningwrtest values (2, 'foo') returning returningwrtest;

