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

SELECT pg_size_pretty(pg_relation_size('large_tuple_test'::regclass, 'main'));

CREATE TABLE IF NOT EXISTS inserttest (f1 int, f2 int[],
                         f3 insert_test_type, f4 insert_test_type[]);

insert into inserttest (f2[1], f2[2]) values (1,2);

insert into inserttest (f2[1], f2[2]) values (3,4), (5,6);

insert into inserttest (f2[1], f2[2]) select 7,8;

insert into inserttest (f2[1], f2[2]) values (1,default);

insert into inserttest (f3.if1, f3.if2) values (1,array['foo']);

insert into inserttest (f3.if1, f3.if2) values (1,'{foo}'), (2,'{bar}');

insert into inserttest (f3.if1, f3.if2) select 3, '{baz,quux}';

insert into inserttest (f3.if1, f3.if2) values (1,default);

insert into inserttest (f3.if2[1], f3.if2[2]) values ('foo', 'bar');

insert into inserttest (f3.if2[1], f3.if2[2]) values ('foo', 'bar'), ('baz', 'quux');

insert into inserttest (f3.if2[1], f3.if2[2]) select 'bear', 'beer';

insert into inserttest (f4[1].if2[1], f4[1].if2[2]) values ('foo', 'bar');

insert into inserttest (f4[1].if2[1], f4[1].if2[2]) values ('foo', 'bar'), ('baz', 'quux');

insert into inserttest (f4[1].if2[1], f4[1].if2[2]) select 'bear', 'beer';

select * from inserttest;

CREATE TABLE IF NOT EXISTS inserttest2 (f1 bigint, f2 text);

CREATE TABLE IF NOT EXISTS inserttesta (f1 int, f2 insert_pos_ints);

CREATE TABLE IF NOT EXISTS inserttestb (f3 insert_test_domain, f4 insert_test_domain[]);

insert into inserttesta (f2[1], f2[2]) values (1,2);

insert into inserttesta (f2[1], f2[2]) values (3,4), (5,6);

insert into inserttesta (f2[1], f2[2]) select 7,8;

insert into inserttesta (f2[1], f2[2]) values (1,default);

insert into inserttesta (f2[1], f2[2]) values (0,2);

insert into inserttesta (f2[1], f2[2]) values (3,4), (0,6);

insert into inserttesta (f2[1], f2[2]) select 0,8;

insert into inserttestb (f3.if1, f3.if2) values (1,array['foo']);

insert into inserttestb (f3.if1, f3.if2) values (1,'{foo}'), (2,'{bar}');

insert into inserttestb (f3.if1, f3.if2) select 3, '{baz,quux}';

insert into inserttestb (f3.if1, f3.if2) values (1,default);

insert into inserttestb (f3.if1, f3.if2) values (1,array[null]);

insert into inserttestb (f3.if1, f3.if2) values (1,'{null}'), (2,'{bar}');

insert into inserttestb (f3.if1, f3.if2) select 3, '{null,quux}';

insert into inserttestb (f3.if2[1], f3.if2[2]) values ('foo', 'bar');

insert into inserttestb (f3.if2[1], f3.if2[2]) values ('foo', 'bar'), ('baz', 'quux');

insert into inserttestb (f3.if2[1], f3.if2[2]) select 'bear', 'beer';

insert into inserttestb (f3, f4[1].if2[1], f4[1].if2[2]) values (row(1,'{x}'), 'foo', 'bar');

insert into inserttestb (f3, f4[1].if2[1], f4[1].if2[2]) values (row(1,'{x}'), 'foo', 'bar'), (row(2,'{y}'), 'baz', 'quux');

insert into inserttestb (f3, f4[1].if2[1], f4[1].if2[2]) select row(1,'{x}')::insert_test_domain, 'bear', 'beer';

select * from inserttesta;

select * from inserttestb;

CREATE TABLE IF NOT EXISTS inserttest2 (f1 bigint, f2 text);

CREATE TABLE IF NOT EXISTS inserttesta (f1 insert_nnarray);

insert into inserttesta (f1[1]) values (1);

insert into inserttesta (f1[1], f1[2]) values (1, 2);

CREATE TABLE IF NOT EXISTS inserttestb (f1 insert_test_domain);

insert into inserttestb (f1.if1) values (1);

insert into inserttestb (f1.if1, f1.if2) values (1, '{foo}');

insert into part1 values ('b', 1);

insert into part1 values ('a', 1);

insert into part4 values ('a', 10);

insert into part4 values ('b', 10);

insert into list_parted values ('ab', 21);

insert into list_parted values ('xx', 1);

insert into list_parted values ('yy', 2);

insert into list_parted values (null, 1);

insert into list_parted (a) values ('aA');

insert into list_parted values ('EE', 1);

insert into list_parted values ('aa'), ('cc');

insert into list_parted select 'Ff', s.a from generate_series(1, 29) s(a);

insert into list_parted select 'gg', s.a from generate_series(1, 9) s(a);

insert into list_parted (b) values (1);

insert into hash_parted values(generate_series(1,10));

insert into hpart0 values(12),(16);

insert into hpart3 values(11);

CREATE TABLE IF NOT EXISTS mlparted11 (like mlparted1);

select attrelid::regclass, attname, attnum
from pg_attribute
where attname = 'a'
 and (attrelid = 'mlparted'::regclass
   or attrelid = 'mlparted1'::regclass
   or attrelid = 'mlparted11'::regclass)
order by attrelid::regclass::text;

insert into mlparted values (1, 2);

end;

insert into mlparted1 (a, b) values (2, 3);

insert into lparted_nonullpart values (1);

CREATE TABLE IF NOT EXISTS mlparted2 (b int not null, a int not null);

CREATE TABLE IF NOT EXISTS mlparted4 (like mlparted);

CREATE TABLE IF NOT EXISTS mlparted5a (a int not null, c text, b int not null);

insert into mlparted values (1, 45, 'a');

insert into mlparted5 (a, b, c) values (1, 40, 'a');

insert into mlparted values (40, 100);

insert into mlparted_def1 values (42, 100);

insert into mlparted_def2 values (54, 50);

insert into mlparted_def1 values (52, 50);

insert into mlparted_def2 values (34, 50);

insert into mlparted values (70, 100);

CREATE TABLE IF NOT EXISTS mlparted5_b (d int, b int, c text, a int);

insert into mlparted values (1, 2, 'a', 1);

insert into mlparted values (1, 40, 'a', 1);

insert into mlparted values (1, 45, 'b', 1);

insert into mlparted values (1, 45, 'c', 1);

insert into mlparted values (1, 45, 'f', 1);

insert into mlparted values (1, 2, 'a', 1);

insert into mlparted values (1, 40, 'a', 1);

insert into mlparted values (1, 45, 'b', 1);

insert into mlparted values (1, 45, 'c', 1);

insert into mlparted values (1, 45, 'f', 1);

set role regress_insert_other_user;

insert into key_desc values (1, 1);

reset role;

set role regress_insert_other_user;

insert into key_desc values (1, 1);

insert into key_desc values (2, 1);

reset role;

drop role regress_insert_other_user;

insert into mcrparted values (null, null, null);

insert into mcrparted values (0, 1, 1);

insert into mcrparted0 values (0, 1, 1);

insert into mcrparted values (9, 1000, 1);

insert into mcrparted1 values (9, 1000, 1);

insert into mcrparted values (10, 5, -1);

insert into mcrparted1 values (10, 5, -1);

insert into mcrparted values (2, 1, 0);

insert into mcrparted1 values (2, 1, 0);

insert into mcrparted values (10, 6, 1000);

insert into mcrparted2 values (10, 6, 1000);

insert into mcrparted values (10, 1000, 1000);

insert into mcrparted2 values (10, 1000, 1000);

insert into mcrparted values (11, 1, -1);

insert into mcrparted3 values (11, 1, -1);

insert into mcrparted values (30, 21, 20);

insert into mcrparted5 values (30, 21, 20);

insert into mcrparted4 values (30, 21, 20);

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

insert into mcrparted values ('aaa', 0), ('b', 0), ('bz', 10), ('c', -10),
    ('comm', -10), ('common', -10), ('common', 0), ('common', 10),
    ('commons', 0), ('d', -10), ('e', 0);

insert into returningwrtest values (1) returning returningwrtest;

CREATE TABLE IF NOT EXISTS returningwrtest2 (b text, c int, a int);

insert into returningwrtest values (2, 'foo') returning returningwrtest;

