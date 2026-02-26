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
  FROM J1_TBL NATURAL JOIN J2_TBL;

SELECT *
  FROM J1_TBL t1 (a, b, c) NATURAL JOIN J2_TBL t2 (a, d);

SELECT *
  FROM J1_TBL t1 (a, b, c) NATURAL JOIN J2_TBL t2 (d, a);

SELECT *
  FROM J1_TBL t1 (a, b) NATURAL JOIN J2_TBL t2 (a);

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

select count(*) from tenk1 a where unique1 in
  (select unique1 from tenk1 b join tenk1 c using (unique1)
   where b.unique2 = 42);

select count(*) from tenk1 x where
  x.unique1 in (select a.f1 from int4_tbl a,float8_tbl b where a.f1=b.f1) and
  x.unique1 = 0 and
  x.unique1 in (select aa.f1 from int4_tbl aa,float8_tbl bb where aa.f1=bb.f1);

rollback;

select aa, bb, unique1, unique1
  from tenk1 right join b_star on aa = unique1
  where bb < bb and bb is null;

select t1.f1
from int4_tbl t1, int4_tbl t2
  left join int4_tbl t3 on t3.f1 > 0
  left join int4_tbl t4 on t3.f1 > 1
where t4.f1 is null;

set enable_hashjoin = 0;

set enable_nestloop = 0;

set enable_hashagg = 0;

reset enable_hashagg;

reset enable_nestloop;

reset enable_hashjoin;

CREATE TEMP TABLE t1 (a int, b int);

CREATE TEMP TABLE t2 (a int, b int);

CREATE TEMP TABLE t3 (x int, y int);

INSERT INTO t1 VALUES (5, 10);

INSERT INTO t1 VALUES (15, 20);

INSERT INTO t1 VALUES (100, 100);

INSERT INTO t1 VALUES (200, 1000);

INSERT INTO t2 VALUES (200, 2000);

INSERT INTO t3 VALUES (5, 20);

INSERT INTO t3 VALUES (6, 7);

INSERT INTO t3 VALUES (7, 8);

INSERT INTO t3 VALUES (500, 100);

DELETE FROM t3 USING t1 table1 WHERE t3.x = table1.a;

SELECT * FROM t3;

SELECT * FROM t3;

DELETE FROM t3 USING t3 t3_other WHERE t3.x = t3_other.x AND t3.y = t3_other.y;

SELECT * FROM t3;

CREATE TABLE IF NOT EXISTS t2a () inherits (t2);

insert into t2a values (200, 2001);

select * from t1 left join t2 on (t1.a = t2.a);

select t1.x from t1 join t3 on (t1.a = t3.x);

select t1.*, t2.*, unnamed_join.* from
  t1 join t2 on (t1.a = t2.a), t3 as unnamed_join
  for update of unnamed_join;

CREATE TEMP TABLE tt1 ( tt1_id int4, joincol int4 );

INSERT INTO tt1 VALUES (1, 11);

INSERT INTO tt1 VALUES (2, NULL);

CREATE TEMP TABLE tt2 ( tt2_id int4, joincol int4 );

INSERT INTO tt2 VALUES (21, 11);

INSERT INTO tt2 VALUES (22, 11);

set enable_hashjoin to off;

set enable_nestloop to off;

select tt1.*, tt2.* from tt1 left join tt2 on tt1.joincol = tt2.joincol;

select tt1.*, tt2.* from tt2 right join tt1 on tt1.joincol = tt2.joincol;

reset enable_hashjoin;

reset enable_nestloop;

CREATE TABLE IF NOT EXISTS tbl_ra(a int unique, b int);

insert into tbl_ra select i, i%100 from generate_series(1,1000)i;

analyze tbl_ra;

set enable_hashjoin to off;

set enable_nestloop to off;

reset enable_hashjoin;

reset enable_nestloop;

CREATE TABLE IF NOT EXISTS tbl_rs(a int, b int);

insert into tbl_rs select i, i from generate_series(1,10)i;

analyze tbl_rs;

set local parallel_setup_cost=0;

set local parallel_tuple_cost=0;

set local min_parallel_table_scan_size=0;

set local max_parallel_workers_per_gather=4;

rollback;

set work_mem to '64kB';

set enable_mergejoin to off;

set enable_memoize to off;

select count(*) from tenk1 a, tenk1 b
  where a.hundred = b.thousand and (b.fivethous % 10) < 10;

reset work_mem;

reset enable_mergejoin;

reset enable_memoize;

CREATE TABLE IF NOT EXISTS tt3(f1 int, f2 text);

insert into tt3 select x, repeat('xyzzy', 100) from generate_series(1,10000) x;

analyze tt3;

CREATE TABLE IF NOT EXISTS tt4(f1 int);

insert into tt4 values (0),(1),(9999);

analyze tt4;

set enable_nestloop to off;

reset enable_nestloop;

set enable_memoize to off;

reset enable_memoize;

CREATE TABLE IF NOT EXISTS tt4x(c1 int, c2 int, c3 int);

CREATE TABLE IF NOT EXISTS tt5(f1 int, f2 int);

CREATE TABLE IF NOT EXISTS tt6(f1 int, f2 int);

insert into tt5 values(1, 10);

insert into tt5 values(1, 11);

insert into tt6 values(1, 9);

insert into tt6 values(1, 2);

insert into tt6 values(2, 9);

select * from tt5,tt6 where tt5.f1 = tt6.f1 and tt5.f1 = tt5.f2 - tt6.f2;

CREATE TABLE IF NOT EXISTS xx (pkxx int);

CREATE TABLE IF NOT EXISTS yy (pkyy int, pkxx int);

insert into xx values (1);

insert into xx values (2);

insert into xx values (3);

insert into yy values (101, 1);

insert into yy values (201, 2);

insert into yy values (301, NULL);

CREATE TABLE IF NOT EXISTS zt1 (f1 int primary key);

CREATE TABLE IF NOT EXISTS zt2 (f2 int primary key);

CREATE TABLE IF NOT EXISTS zt3 (f3 int primary key);

insert into zt1 values(53);

insert into zt2 values(53);

select * from
  zt2 left join zt3 on (f2 = f3)
      left join zt1 on (f3 = f1)
where f2 = 53;

create temp view zv1 as select *,'dummy'::text AS junk from zt1;

select * from
  zt2 left join zt3 on (f2 = f3)
      left join zv1 on (f3 = f1)
where f2 = 53;

select a.unique2, a.ten, b.tenthous, b.unique2, b.hundred
from tenk1 a left join tenk1 b on a.unique2 = b.tenthous
where a.unique1 = 42 and
      ((b.unique2 is null and a.ten = 2) or b.hundred = 3);

prepare foo(bool) as
  select count(*) from tenk1 a left join tenk1 b
    on (a.unique2 = b.unique1 and exists
        (select 1 from tenk1 c where c.thousand = b.unique2 and $1));

execute foo(true);

execute foo(false);

set enable_mergejoin = 1;

set enable_hashjoin = 0;

set enable_nestloop = 0;

CREATE TABLE IF NOT EXISTS a (i integer);

CREATE TABLE IF NOT EXISTS b (x integer, y integer);

select * from a left join b on i = x and i = y and x = i;

rollback;

CREATE TABLE IF NOT EXISTS tidv (idv mycomptype);

set enable_mergejoin = 0;

set enable_hashjoin = 0;

rollback;

select t1.q2, count(t2.*)
from int8_tbl t1 left join int8_tbl t2 on (t1.q2 = t2.q1)
group by t1.q2 order by 1;

CREATE TABLE IF NOT EXISTS a (
     code char not null,
     constraint a_pk primary key (code)
);

CREATE TABLE IF NOT EXISTS b (
     a char not null,
     num integer not null,
     constraint b_pk primary key (a, num)
);

CREATE TABLE IF NOT EXISTS c (
     name char not null,
     a char,
     constraint c_pk primary key (name)
);

insert into a (code) values ('p');

insert into a (code) values ('q');

insert into b (a, num) values ('p', 1);

insert into b (a, num) values ('p', 2);

insert into c (name, a) values ('A', 'p');

insert into c (name, a) values ('B', 'q');

insert into c (name, a) values ('C', null);

rollback;

CREATE TABLE IF NOT EXISTS nt1 (
  id int primary key,
  a1 boolean,
  a2 boolean
);

CREATE TABLE IF NOT EXISTS nt2 (
  id int primary key,
  nt1_id int,
  b1 boolean,
  b2 boolean,
  foreign key (nt1_id) references nt1(id)
);

CREATE TABLE IF NOT EXISTS nt3 (
  id int primary key,
  nt2_id int,
  c1 boolean,
  foreign key (nt2_id) references nt2(id)
);

insert into nt1 values (1,true,true);

insert into nt1 values (2,true,false);

insert into nt1 values (3,false,false);

insert into nt2 values (1,1,true,true);

insert into nt2 values (2,2,true,false);

insert into nt2 values (3,3,false,false);

insert into nt3 values (1,1,true);

insert into nt3 values (2,2,false);

insert into nt3 values (3,3,true);

select * from int4_tbl a full join int4_tbl b on true;

select * from int4_tbl a full join int4_tbl b on false;

CREATE TABLE IF NOT EXISTS q1 as select 1 as q1;

CREATE TABLE IF NOT EXISTS q2 as select 0 as q2;

analyze q1;

analyze q2;

set local from_collapse_limit to 2;

rollback;

CREATE TABLE IF NOT EXISTS t(i int primary key);

rollback;

with ctetable as not materialized ( select 1 as f1 )
select * from ctetable c1
where f1 in ( select c3.f1 from ctetable c2 full join ctetable c3 on true );

select * from mki8(1,2);

select * from mki4(42);

select count(*) from
  tenk1 a join tenk1 b on a.unique1 = b.unique2
  left join tenk1 c on a.unique2 = b.unique1 and c.thousand = a.thousand
  join int4_tbl on b.thousand = f1;

select b.unique1 from
  tenk1 a join tenk1 b on a.unique1 = b.unique2
  left join tenk1 c on b.unique1 = 42 and c.thousand = a.thousand
  join int4_tbl i1 on b.thousand = f1
  right join int4_tbl i2 on i2.f1 = b.tenthous
  order by 1;

select q1, unique2, thousand, hundred
  from int8_tbl a left join tenk1 b on q1 = unique2
  where coalesce(thousand,123) = q1 and q1 = coalesce(hundred,123);

select f1, unique2, case when unique2 is null then f1 else 0 end
  from int4_tbl a left join tenk1 b on f1 = unique2
  where (case when unique2 is null then f1 else 0 end) = 0;

select a.unique1, b.unique1, c.unique1, coalesce(b.twothousand, a.twothousand)
  from tenk1 a left join tenk1 b on b.thousand = a.unique1                        left join tenk1 c on c.unique2 = coalesce(b.twothousand, a.twothousand)
  where a.unique2 < 10 and coalesce(b.twothousand, a.twothousand) = 44;

select * from
  text_tbl t1
  inner join int8_tbl i8
  on i8.q2 = 456
  right join text_tbl t2
  on t1.f1 = 'doh!'
  left join int4_tbl i4
  on i8.q1 = i4.f1;

CREATE TABLE IF NOT EXISTS t (a int unique);

rollback;

set enable_hashjoin to off;

set enable_nestloop to off;

select a.q2, b.q1
  from int8_tbl a left join int8_tbl b on a.q2 = coalesce(b.q1, 1)
  where coalesce(b.q1, 1) > 0;

reset enable_hashjoin;

reset enable_nestloop;

select a.unique1, b.unique2
  from onek a left join onek b on a.unique1 = b.unique2
  where (b.unique2, random() > 0) = any (select q1, random() > 0 from int8_tbl c where c.q1 < b.unique1);

select a.unique1, b.unique2
  from onek a full join onek b on a.unique1 = b.unique2
  where a.unique1 = 42;

select a.unique1, b.unique2
  from onek a full join onek b on a.unique1 = b.unique2
  where b.unique2 = 43;

select a.unique1, b.unique2
  from onek a full join onek b on a.unique1 = b.unique2
  where a.unique1 = 42 and b.unique2 = 42;

CREATE TEMP TABLE a (id int PRIMARY KEY, b_id int);

CREATE TEMP TABLE b (id int PRIMARY KEY, c_id int);

CREATE TEMP TABLE c (id int PRIMARY KEY);

CREATE TEMP TABLE d (a int, b int);

CREATE TEMP TABLE e (id1 int, id2 int, PRIMARY KEY(id1, id2));

INSERT INTO a VALUES (0, 0), (1, NULL);

INSERT INTO b VALUES (0, 0), (1, NULL);

INSERT INTO c VALUES (0), (1);

INSERT INTO d VALUES (1,3), (2,2), (3,1);

INSERT INTO e VALUES (0,0), (2,2), (3,1);

rollback;

CREATE TABLE IF NOT EXISTS parent (k int primary key, pd int);

CREATE TABLE IF NOT EXISTS child (k int unique, cd int);

insert into parent values (1, 10), (2, 20), (3, 30);

insert into child values (1, 100), (4, 400);

select p.* from parent p left join child c on (p.k = c.k);

select p.* from
  parent p left join child c on (p.k = c.k)
  where p.k = 1 and p.k = 2;

CREATE TEMP TABLE a (id int PRIMARY KEY);

CREATE TEMP TABLE b (id int PRIMARY KEY, a_id int);

INSERT INTO a VALUES (0), (1);

INSERT INTO b VALUES (0, 0), (1, NULL);

SELECT * FROM b LEFT JOIN a ON (b.a_id = a.id) WHERE (a.id IS NULL OR a.id > 0);

SELECT b.* FROM b LEFT JOIN a ON (b.a_id = a.id) WHERE (a.id IS NULL OR a.id > 0);

rollback;

CREATE TABLE IF NOT EXISTS innertab (id int8 primary key, dat1 int8);

insert into innertab values(123, 42);

rollback;

CREATE TABLE IF NOT EXISTS uniquetbl (f1 text unique);

rollback;

CREATE TABLE IF NOT EXISTS t (a int unique);

insert into t values (1);

rollback;

CREATE TABLE IF NOT EXISTS t (a int unique, b int);

insert into t values (1, 2);

select t1.a from t t1
  left join t t2 on t1.a = t2.a
       join t t3 on true
where exists (select 1 from t t4
                join t t5 on t4.b = t5.b
                join t t6 on t5.b = t6.b
              where t1.a = t4.a and t3.a = t5.a and t4.a = 1);

rollback;

CREATE TABLE IF NOT EXISTS t (a int, b int);

insert into t values (1, 2);

select * from t t1, t t2 where exists
  (select 1 from t t3 where t1.a = t3.a and t2.b = t3.b and t3.a = 1 and t3.b = 2);

rollback;

insert into p values (1, 2);

insert into p values (10, 20);

set enable_partitionwise_join to on;

select * from p t1 where exists
  (select 1 from p t2 where t1.a = t2.a and t1.a = 1);

rollback;

CREATE TABLE IF NOT EXISTS t (a int unique, b int);

insert into t values (1,1), (2,2);

rollback;

insert into parttbl values (11), (12);

select * from
  int8_tbl x join (int4_tbl x cross join int4_tbl y) j on q1 = f1;

select * from
  int8_tbl x join (int4_tbl x cross join int4_tbl y) j on q1 = y.f1;

select * from
  int8_tbl x join (int4_tbl x cross join int4_tbl y(ff)) j on q1 = f1;

set enable_hashjoin to off;

set enable_mergejoin to off;

CREATE TABLE IF NOT EXISTS sj (a int unique, b int, c int unique);

insert into sj values (1, null, 2), (null, 2, null), (2, 1, 1);

analyze sj;

select p.* from sj p, sj q where q.a = p.a and q.b = q.a - 1;

select * from sj p
where exists (select * from sj q
              where q.a = p.a and q.b < 10);

INSERT INTO sj VALUES (3, 1, 3);

SELECT * FROM sj j1, sj j2 WHERE j1.b = j2.b AND j1.a = 2 AND j2.a = 3;

SELECT * FROM sj j1, sj j2 WHERE j1.b = j2.b AND j1.a = 2 AND j2.a = 2;

SELECT * FROM sj j1, sj j2
WHERE j1.b = j2.b
  AND j1.a = (EXTRACT(DOW FROM current_timestamp(0))/15 + 3)::int
  AND (EXTRACT(DOW FROM current_timestamp(0))/15 + 3)::int = j2.a;

SELECT * FROM sj j1, sj j2 WHERE j1.b = j2.b AND j1.a = 1 AND j2.a = 1;

SELECT * FROM sj j1, sj j2 WHERE j1.b = j2.b AND 1 = j1.a AND j2.a = 1;

CREATE UNIQUE INDEX sj_fn_idx ON sj((a * a));

SELECT * FROM sj j1, sj j2
WHERE j1.b = j2.b
  AND (j1.a*j1.a) = (EXTRACT(DOW FROM current_timestamp(0))/15 + 3)::int
  AND (EXTRACT(DOW FROM current_timestamp(0))/15 + 3)::int = (j2.a*j2.a);

SELECT * FROM sj j1, sj j2
WHERE j1.b = j2.b
  AND (j1.a*j1.c/3) = (random()/3 + 3)::int
  AND (random()/3 + 3)::int = (j2.a*j2.c/3);

CREATE UNIQUE INDEX sj_temp_idx1 ON sj(a,b,c);

CREATE UNIQUE INDEX sj_temp_idx ON sj(a,b);

CREATE TABLE tab_with_flag ( id INT PRIMARY KEY, is_flag SMALLINT);

CREATE TABLE IF NOT EXISTS sk (a int, b int);

set join_collapse_limit to 1;

set enable_seqscan to off;

reset join_collapse_limit;

reset enable_seqscan;

CREATE TABLE emp1 (id SERIAL PRIMARY KEY NOT NULL, code int);

CREATE UNIQUE INDEX ON emp1((id*id));

CREATE TABLE tbl_phv(x int, y int PRIMARY KEY);

INSERT INTO tbl_phv (x, y)
  SELECT gs, gs FROM generate_series(1,100) AS gs;

VACUUM ANALYZE tbl_phv;

INSERT INTO emp1 VALUES (1, 1), (2, 1);

WITH t1 AS (SELECT * FROM emp1)
UPDATE emp1 SET code = t1.code + 1 FROM t1
WHERE t1.id = emp1.id RETURNING emp1.id, emp1.code, t1.code;

insert into emp1 values (1, 1);

insert into emp1 values (2, 1);

select * from emp1 t1 where exists (select * from emp1 t2
                                    where t2.id = t1.code and t2.code > 0);

CREATE TABLE IF NOT EXISTS sl(a int, b int, c int);

create unique index on sl(a, b);

vacuum analyze sl;

CREATE TABLE sj_t1 (id serial, a int);

CREATE TABLE sj_t2 (id serial, a int);

CREATE TABLE sj_t3 (id serial, a int);

CREATE TABLE sj_t4 (id serial, a int);

CREATE UNIQUE INDEX ON sj_t3 USING btree (a,id);

CREATE UNIQUE INDEX ON sj_t2 USING btree (id);

reset enable_hashjoin;

reset enable_mergejoin;

select t1.uunique1 from
  tenk1 t1 join tenk2 t2 on t1.two = t2.two;

select t2.uunique1 from
  tenk1 t1 join tenk2 t2 on t1.two = t2.two;

select uunique1 from
  tenk1 t1 join tenk2 t2 on t1.two = t2.two;

select ctid from
  tenk1 t1 join tenk2 t2 on t1.two = t2.two;

CREATE TABLE IF NOT EXISTS xx1 as select f1 as x1, -f1 as x2 from int4_tbl;

delete from xx1 using (select * from int4_tbl where f1 = xx1.x1) ss;

insert into join_pt1 values (1, 1, 'x'), (101, 101, 'y');

CREATE TABLE IF NOT EXISTS join_ut1 (a int, b int, c varchar);

insert into join_ut1 values (101, 101, 'y'), (2, 2, 'z');

CREATE TABLE IF NOT EXISTS fkest (x integer, x10 integer, x10b integer, x100 integer);

insert into fkest select x, x/10, x/10, x/100 from generate_series(1,1000) x;

create unique index on fkest(x, x10, x100);

analyze fkest;

rollback;

CREATE TABLE IF NOT EXISTS fkest (a int, b int, c int unique, primary key(a,b));

CREATE TABLE IF NOT EXISTS fkest1 (a int, b int, primary key(a,b));

insert into fkest select x/10, x%10, x from generate_series(1,1000) x;

insert into fkest1 select x/10, x%10 from generate_series(1,1000) x;

analyze fkest;

analyze fkest1;

rollback;

CREATE TABLE IF NOT EXISTS j1 (id int primary key);

CREATE TABLE IF NOT EXISTS j2 (id int primary key);

CREATE TABLE IF NOT EXISTS j3 (id int);

insert into j1 values(1),(2),(3);

insert into j2 values(1),(2),(3);

insert into j3 values(1),(1);

analyze j1;

analyze j2;

analyze j3;

CREATE TABLE IF NOT EXISTS j1 (id1 int, id2 int, primary key(id1,id2));

CREATE TABLE IF NOT EXISTS j2 (id1 int, id2 int, primary key(id1,id2));

CREATE TABLE IF NOT EXISTS j3 (id1 int, id2 int, primary key(id1,id2));

insert into j1 values(1,1),(1,2);

insert into j2 values(1,1);

insert into j3 values(1,1);

analyze j1;

analyze j2;

analyze j3;

create unique index j1_id2_idx on j1(id2) where id2 > 0;

set enable_nestloop to 0;

set enable_hashjoin to 0;

set enable_sort to 0;

insert into j2 values(1,2);

analyze j2;

select * from j1
inner join j2 on j1.id1 = j2.id1 and j1.id2 = j2.id2
where j1.id1 % 1000 = 1 and j2.id1 % 1000 = 1;

select * from j1
inner join j2 on j1.id1 = j2.id1 and j1.id2 = j2.id2
where j1.id1 % 1000 = 1 and j2.id1 % 1000 = 1 and j2.id1 = any (array[1]);

select * from j1
inner join j2 on j1.id1 = j2.id1 and j1.id2 = j2.id2
where j1.id1 % 1000 = 1 and j2.id1 % 1000 = 1 and j2.id1 >= any (array[1,5]);

reset enable_nestloop;

reset enable_hashjoin;

reset enable_sort;

CREATE TABLE IF NOT EXISTS j3 as select unique1, tenthous from onek;

vacuum analyze j3;

create unique index on j3(unique1, tenthous);

CREATE TEMP TABLE skip_fetch (a INT, b INT) WITH (fillfactor=10);

INSERT INTO skip_fetch SELECT i % 3, i FROM generate_series(0,30) i;

VACUUM (ANALYZE) skip_fetch;

SET enable_indexonlyscan = off;

SET enable_seqscan = off;

SELECT t1.a FROM skip_fetch t1 LEFT JOIN skip_fetch t2 ON t2.a = 1 WHERE t2.a IS NULL;

RESET enable_indexonlyscan;

RESET enable_seqscan;

SET enable_seqscan = off;

SET enable_indexscan = off;

CREATE TEMP TABLE rescan_bhs (a INT);

INSERT INTO rescan_bhs VALUES (1), (2);

SELECT * FROM rescan_bhs t1 LEFT JOIN rescan_bhs t2 ON t1.a IN
  (SELECT a FROM rescan_bhs t3 WHERE t2.a > 1);

RESET enable_seqscan;

RESET enable_indexscan;

CREATE TABLE group_tbl (a INT, b INT);

INSERT INTO group_tbl SELECT 1, 1;

CREATE STATISTICS group_tbl_stat (ndistinct) ON a, b FROM group_tbl;

ANALYZE group_tbl;

SELECT COUNT(*) FROM tenk1 t1, tenk1 t2
WHERE t2.thousand = t1.tenthous OR t2.thousand = t1.unique1 OR t2.thousand = t1.unique2;

SELECT COUNT(*) FROM onek t1 LEFT JOIN tenk1 t2
    ON (t2.thousand = t1.tenthous OR t2.thousand = t1.thousand);

