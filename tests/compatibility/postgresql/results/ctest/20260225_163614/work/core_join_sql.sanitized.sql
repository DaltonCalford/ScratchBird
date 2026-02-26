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

create temp table onerow();

insert into onerow default values;

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

SELECT ii, tt, kk
  FROM (J1_TBL CROSS JOIN J2_TBL)
    AS tx (ii, jj, tt, ii2, kk);

SELECT tx.ii, tx.jj, tx.kk
  FROM (J1_TBL t1 (a, b, c) CROSS JOIN J2_TBL t2 (d, e))
    AS tx (ii, jj, tt, ii2, kk);

SELECT *
  FROM J1_TBL CROSS JOIN J2_TBL a CROSS JOIN J2_TBL b;

SELECT *
  FROM J1_TBL INNER JOIN J2_TBL USING (i);

SELECT *
  FROM J1_TBL JOIN J2_TBL USING (i);

SELECT *
  FROM J1_TBL t1 (a, b, c) JOIN J2_TBL t2 (a, d) USING (a)
  ORDER BY a, d;

SELECT *
  FROM J1_TBL t1 (a, b, c) JOIN J2_TBL t2 (a, b) USING (b)
  ORDER BY b, t1.a;

SELECT * FROM J1_TBL JOIN J2_TBL USING (i) WHERE J1_TBL.t = 'one';  -- ok

SELECT * FROM J1_TBL JOIN J2_TBL USING (i) AS x WHERE J1_TBL.t = 'one';  -- ok

SELECT * FROM (J1_TBL JOIN J2_TBL USING (i)) AS x WHERE J1_TBL.t = 'one';  -- error

SELECT * FROM J1_TBL JOIN J2_TBL USING (i) AS x WHERE x.i = 1;  -- ok

SELECT * FROM J1_TBL JOIN J2_TBL USING (i) AS x WHERE x.t = 'one';  -- error

SELECT * FROM (J1_TBL JOIN J2_TBL USING (i) AS x) AS xx WHERE x.i = 1;  -- error (XXX could use better hint)

SELECT * FROM J1_TBL a1 JOIN J2_TBL a2 USING (i) AS a1;  -- error

SELECT x.* FROM J1_TBL JOIN J2_TBL USING (i) AS x WHERE J1_TBL.t = 'one';

SELECT ROW(x.*) FROM J1_TBL JOIN J2_TBL USING (i) AS x WHERE J1_TBL.t = 'one';

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

create temp table x (x1 int, x2 int);

insert into x values (1,11);

insert into x values (2,22);

insert into x values (3,null);

insert into x values (4,44);

insert into x values (5,null);

create temp table y (y1 int, y2 int);

insert into y values (1,111);

insert into y values (2,222);

insert into y values (3,333);

insert into y values (4,null);

select * from x;

select * from y;

select * from x left join y on (x1 = y1 and x2 is not null);

select * from x left join y on (x1 = y1 and y2 is not null);

select * from (x left join y on (x1 = y1)) left join x xx(xx1,xx2)
on (x1 = xx1);

select * from (x left join y on (x1 = y1)) left join x xx(xx1,xx2)
on (x1 = xx1 and x2 is not null);

select * from (x left join y on (x1 = y1)) left join x xx(xx1,xx2)
on (x1 = xx1 and y2 is not null);

select * from (x left join y on (x1 = y1)) left join x xx(xx1,xx2)
on (x1 = xx1 and xx2 is not null);

select * from (x left join y on (x1 = y1)) left join x xx(xx1,xx2)
on (x1 = xx1) where (x2 is not null);

select * from (x left join y on (x1 = y1)) left join x xx(xx1,xx2)
on (x1 = xx1) where (y2 is not null);

select * from (x left join y on (x1 = y1)) left join x xx(xx1,xx2)
on (x1 = xx1) where (xx2 is not null);

select count(*) from tenk1 a where unique1 in
  (select unique1 from tenk1 b join tenk1 c using (unique1)
   where b.unique2 = 42);

begin;

set geqo = on;

set geqo_threshold = 2;

select count(*) from tenk1 x where
  x.unique1 in (select a.f1 from int4_tbl a,float8_tbl b where a.f1=b.f1) and
  x.unique1 = 0 and
  x.unique1 in (select aa.f1 from int4_tbl aa,float8_tbl bb where aa.f1=bb.f1);

rollback;

select aa, bb, unique1, unique1
  from tenk1 right join b_star on aa = unique1
  where bb < bb and bb is null;

select * from int8_tbl i1 left join (int8_tbl i2 join
  (select 123 as x) ss on i2.q1 = x) on i1.q2 = i2.q2
order by 1, 2;

select a.f1, b.f1, t.thousand, t.tenthous from
  tenk1 t,
  (select sum(f1)+1 as f1 from int4_tbl i4a) a,
  (select sum(f1) as f1 from int4_tbl i4b) b
where b.f1 = t.thousand and a.f1 = b.f1 and (a.f1+b.f1+999) = t.tenthous;

select t1.f1
from int4_tbl t1, int4_tbl t2
  left join int4_tbl t3 on t3.f1 > 0
  left join int4_tbl t4 on t3.f1 > 1
where t4.f1 is null;

select * from
  j1_tbl full join
  (select * from j2_tbl order by j2_tbl.i desc, j2_tbl.k asc) j2_tbl
  on j1_tbl.i = j2_tbl.i and j1_tbl.i = j2_tbl.k;

set enable_hashjoin = 0;

set enable_nestloop = 0;

set enable_hashagg = 0;

reset enable_hashagg;

reset enable_nestloop;

reset enable_hashjoin;

DROP TABLE t1;

DROP TABLE t2;

DROP TABLE t3;

DROP TABLE J1_TBL;

DROP TABLE J2_TBL;

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

DELETE FROM t3 USING t1 JOIN t2 USING (a) WHERE t3.x > t1.a;

SELECT * FROM t3;

DELETE FROM t3 USING t3 t3_other WHERE t3.x = t3_other.x AND t3.y = t3_other.y;

SELECT * FROM t3;

create temp table t2a () inherits (t2);

insert into t2a values (200, 2001);

select * from t1 left join t2 on (t1.a = t2.a);

select t1.x from t1 join t3 on (t1.a = t3.x);

select t1.*, t2.*, unnamed_join.* from
  t1 join t2 on (t1.a = t2.a), t3 as unnamed_join
  for update of unnamed_join;

select foo.*, unnamed_join.* from
  t1 join t2 using (a) as foo, t3 as unnamed_join
  for update of unnamed_join;

select foo.*, unnamed_join.* from
  t1 join t2 using (a) as foo, t3 as unnamed_join
  for update of foo;

select bar.*, unnamed_join.* from
  (t1 join t2 using (a) as foo) as bar, t3 as unnamed_join
  for update of foo;

select bar.*, unnamed_join.* from
  (t1 join t2 using (a) as foo) as bar, t3 as unnamed_join
  for update of bar;

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

create temp table tbl_ra(a int unique, b int);

insert into tbl_ra select i, i%100 from generate_series(1,1000)i;

create index on tbl_ra (b);

analyze tbl_ra;

set enable_hashjoin to off;

set enable_nestloop to off;

reset enable_hashjoin;

reset enable_nestloop;

create temp table tbl_rs(a int, b int);

insert into tbl_rs select i, i from generate_series(1,10)i;

analyze tbl_rs;

begin;

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

create temp table tt3(f1 int, f2 text);

insert into tt3 select x, repeat('xyzzy', 100) from generate_series(1,10000) x;

analyze tt3;

create temp table tt4(f1 int);

insert into tt4 values (0),(1),(9999);

analyze tt4;

set enable_nestloop to off;

SELECT a.f1
FROM tt4 a
LEFT JOIN (
        SELECT b.f1
        FROM tt3 b LEFT JOIN tt3 c ON (b.f1 = c.f1)
        WHERE COALESCE(c.f1, 0) = 0
) AS d ON (a.f1 = d.f1)
WHERE COALESCE(d.f1, 0) = 0
ORDER BY 1;

reset enable_nestloop;

set enable_memoize to off;

reset enable_memoize;

create temp table tt4x(c1 int, c2 int, c3 int);

create temp table tt5(f1 int, f2 int);

create temp table tt6(f1 int, f2 int);

insert into tt5 values(1, 10);

insert into tt5 values(1, 11);

insert into tt6 values(1, 9);

insert into tt6 values(1, 2);

insert into tt6 values(2, 9);

select * from tt5,tt6 where tt5.f1 = tt6.f1 and tt5.f1 = tt5.f2 - tt6.f2;

create temp table xx (pkxx int);

create temp table yy (pkyy int, pkxx int);

insert into xx values (1);

insert into xx values (2);

insert into xx values (3);

insert into yy values (101, 1);

insert into yy values (201, 2);

insert into yy values (301, NULL);

select yy.pkyy as yy_pkyy, yy.pkxx as yy_pkxx, yya.pkyy as yya_pkyy,
       xxa.pkxx as xxa_pkxx, xxb.pkxx as xxb_pkxx
from yy
     left join (SELECT * FROM yy where pkyy = 101) as yya ON yy.pkyy = yya.pkyy
     left join xx xxa on yya.pkxx = xxa.pkxx
     left join xx xxb on coalesce (xxa.pkxx, 1) = xxb.pkxx;

create temp table zt1 (f1 int primary key);

create temp table zt2 (f2 int primary key);

create temp table zt3 (f3 int primary key);

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

begin;

set enable_mergejoin = 1;

set enable_hashjoin = 0;

set enable_nestloop = 0;

create temp table a (i integer);

create temp table b (x integer, y integer);

select * from a left join b on i = x and i = y and x = i;

rollback;

begin;

create temp table tidv (idv mycomptype);

create index on tidv (idv);

set enable_mergejoin = 0;

set enable_hashjoin = 0;

rollback;

select t1.q2, count(t2.*)
from int8_tbl t1 left join int8_tbl t2 on (t1.q2 = t2.q1)
group by t1.q2 order by 1;

select t1.q2, count(t2.*)
from int8_tbl t1 left join (select * from int8_tbl) t2 on (t1.q2 = t2.q1)
group by t1.q2 order by 1;

select t1.q2, count(t2.*)
from int8_tbl t1 left join (select * from int8_tbl offset 0) t2 on (t1.q2 = t2.q1)
group by t1.q2 order by 1;

select t1.q2, count(t2.*)
from int8_tbl t1 left join
  (select q1, case when q2=1 then 1 else q2 end as q2 from int8_tbl) t2
  on (t1.q2 = t2.q1)
group by t1.q2 order by 1;

create temp table a (
     code char not null,
     constraint a_pk primary key (code)
);

create temp table b (
     a char not null,
     num integer not null,
     constraint b_pk primary key (a, num)
);

create temp table c (
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

select c.name, ss.code, ss.b_cnt, ss.const
from c left join
  (select a.code, coalesce(b_grp.cnt, 0) as b_cnt, -1 as const
   from a left join
     (select count(1) as cnt, b.a from b group by b.a) as b_grp
     on a.code = b_grp.a
  ) as ss
  on (c.a = ss.code)
order by c.name;

rollback;

create temp table nt1 (
  id int primary key,
  a1 boolean,
  a2 boolean
);

create temp table nt2 (
  id int primary key,
  nt1_id int,
  b1 boolean,
  b2 boolean,
  foreign key (nt1_id) references nt1(id)
);

create temp table nt3 (
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

select nt3.id
from nt3 as nt3
  left join
    (select nt2.*, (nt2.b1 and ss1.a3) AS b3
     from nt2 as nt2
       left join
         (select nt1.*, (nt1.id is not null) as a3 from nt1) as ss1
         on ss1.id = nt2.nt1_id
    ) as ss2
    on ss2.id = nt3.nt2_id
where nt3.id = 1 and ss2.b3;

select * from
  int8_tbl t1 left join
  (select q1 as x, 42 as y from int8_tbl t2) ss
  on t1.q2 = ss.x
where
  1 = (select 1 from int8_tbl t3 where ss.y is not null limit 1)
order by 1,2;

select * from int4_tbl a full join int4_tbl b on true;

select * from int4_tbl a full join int4_tbl b on false;

create temp table q1 as select 1 as q1;

create temp table q2 as select 0 as q2;

analyze q1;

analyze q2;

begin;

set local from_collapse_limit to 2;

rollback;

begin;

create temp table t(i int primary key);

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
int4_tbl i0 left join
( (select *, 123 as x from int4_tbl i1) ss1
  left join
  (select *, q2 as x from int8_tbl i2) ss2
  using (x)
) ss0
on (i0.f1 = ss0.f1)
order by i0.f1, x;

select t1.* from
  text_tbl t1
  left join (select *, '***'::text as d1 from int8_tbl i8b1) b1
    left join int8_tbl i8
      left join (select *, null::int as d2 from int8_tbl i8b2) b2
      on (i8.q1 = b2.q1)
    on (b2.d2 = b1.q2)
  on (t1.f1 = b1.d1)
  left join int4_tbl i4
  on (i8.q2 = i4.f1);

select t1.* from
  text_tbl t1
  left join (select *, '***'::text as d1 from int8_tbl i8b1) b1
    left join int8_tbl i8
      left join (select *, null::int as d2 from int8_tbl i8b2, int4_tbl i4b2) b2
      on (i8.q1 = b2.q1)
    on (b2.d2 = b1.q2)
  on (t1.f1 = b1.d1)
  left join int4_tbl i4
  on (i8.q2 = i4.f1);

select t1.* from
  text_tbl t1
  left join (select *, '***'::text as d1 from int8_tbl i8b1) b1
    left join int8_tbl i8
      left join (select *, null::int as d2 from int8_tbl i8b2, int4_tbl i4b2
                 where q1 = f1) b2
      on (i8.q1 = b2.q1)
    on (b2.d2 = b1.q2)
  on (t1.f1 = b1.d1)
  left join int4_tbl i4
  on (i8.q2 = i4.f1);

select * from
  text_tbl t1
  inner join int8_tbl i8
  on i8.q2 = 456
  right join text_tbl t2
  on t1.f1 = 'doh!'
  left join int4_tbl i4
  on i8.q1 = i4.f1;

begin;

create temp table t (a int unique);

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

begin;

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

CREATE TEMP TABLE parted_b1 partition of parted_b for values from (0) to (10);

rollback;

create temp table parent (k int primary key, pd int);

create temp table child (k int unique, cd int);

insert into parent values (1, 10), (2, 20), (3, 30);

insert into child values (1, 100), (4, 400);

select p.* from parent p left join child c on (p.k = c.k);

select p.*, linked from parent p
  left join (select c.*, true as linked from child c) as ss
  on (p.k = ss.k);

select p.* from
  parent p left join child c on (p.k = c.k)
  where p.k = 1 and p.k = 2;

select p.* from
  (parent p left join child c on (p.k = c.k)) join parent x on p.k = x.k
  where p.k = 1 and p.k = 2;

begin;

CREATE TEMP TABLE a (id int PRIMARY KEY);

CREATE TEMP TABLE b (id int PRIMARY KEY, a_id int);

INSERT INTO a VALUES (0), (1);

INSERT INTO b VALUES (0, 0), (1, NULL);

SELECT * FROM b LEFT JOIN a ON (b.a_id = a.id) WHERE (a.id IS NULL OR a.id > 0);

SELECT b.* FROM b LEFT JOIN a ON (b.a_id = a.id) WHERE (a.id IS NULL OR a.id > 0);

rollback;

begin;

create temp table innertab (id int8 primary key, dat1 int8);

insert into innertab values(123, 42);

rollback;

begin;

create temp table uniquetbl (f1 text unique);

select t0.*
from
 text_tbl t0
 left join
   (select case t1.ten when 0 then 'doh!'::text else null::text end as case1,
           t1.stringu2
     from tenk1 t1
     join int4_tbl i4 ON i4.f1 = t1.unique2
     left join uniquetbl u1 ON u1.f1 = t1.string4) ss
  on t0.f1 = ss.case1
where ss.stringu2 !~* ss.case1;

rollback;

begin;

create temp table t (a int unique);

insert into t values (1);

select 1
from t t1
  left join (select 2 as c
             from t t2 left join t t3 on t2.a = t3.a) s
    on true
where t1.a = s.c;

rollback;

begin;

create temp table t (a int unique, b int);

insert into t values (1, 2);

select t1.a from t t1
  left join t t2 on t1.a = t2.a
       join t t3 on true
where exists (select 1 from t t4
                join t t5 on t4.b = t5.b
                join t t6 on t5.b = t6.b
              where t1.a = t4.a and t3.a = t5.a and t4.a = 1);

rollback;

begin;

create temp table t (a int, b int);

insert into t values (1, 2);

select * from t t1, t t2 where exists
  (select 1 from t t3 where t1.a = t3.a and t2.b = t3.b and t3.a = 1 and t3.b = 2);

rollback;

begin;

create temp table p1 partition of p for values from (0) to (10);

create temp table p2 partition of p for values from (10) to (20);

insert into p values (1, 2);

insert into p values (10, 20);

set enable_partitionwise_join to on;

select * from p t1 where exists
  (select 1 from p t2 where t1.a = t2.a and t1.a = 1);

rollback;

begin;

create temp table t (a int unique, b int);

insert into t values (1,1), (2,2);

rollback;

create temp table parttbl1 partition of parttbl for values from (1) to (100);

insert into parttbl values (11), (12);

select * from
  int8_tbl x join (int4_tbl x cross join int4_tbl y) j on q1 = f1; -- error

select * from
  int8_tbl x join (int4_tbl x cross join int4_tbl y) j on q1 = y.f1; -- error

select * from
  int8_tbl x join (int4_tbl x cross join int4_tbl y(ff)) j on q1 = f1; -- ok

set enable_hashjoin to off;

set enable_mergejoin to off;

create table sj (a int unique, b int, c int unique);

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

DROP INDEX sj_fn_idx, sj_temp_idx1, sj_temp_idx;

CREATE TABLE tab_with_flag ( id INT PRIMARY KEY, is_flag SMALLINT);

CREATE INDEX idx_test_is_flag ON tab_with_flag (is_flag);

DROP TABLE tab_with_flag;

create table sk (a int, b int);

create index on sk(a);

set join_collapse_limit to 1;

set enable_seqscan to off;

reset join_collapse_limit;

reset enable_seqscan;

CREATE TABLE emp1 (id SERIAL PRIMARY KEY NOT NULL, code int);

CREATE UNIQUE INDEX ON emp1((id*id));

CREATE TABLE tbl_phv(x int, y int PRIMARY KEY);

CREATE INDEX tbl_phv_idx ON tbl_phv(x);

INSERT INTO tbl_phv (x, y)
  SELECT gs, gs FROM generate_series(1,100) AS gs;

VACUUM ANALYZE tbl_phv;

DROP TABLE IF EXISTS tbl_phv;

INSERT INTO emp1 VALUES (1, 1), (2, 1);

WITH t1 AS (SELECT * FROM emp1)
UPDATE emp1 SET code = t1.code + 1 FROM t1
WHERE t1.id = emp1.id RETURNING emp1.id, emp1.code, t1.code;

TRUNCATE emp1;

insert into emp1 values (1, 1);

select 1 from emp1 full join
    (select * from emp1 t1 join
        emp1 t2 join emp1 t3 on t2.id = t3.id
        on true
    where false) s on true
where false;

insert into emp1 values (2, 1);

select * from emp1 t1 where exists (select * from emp1 t2
                                    where t2.id = t1.code and t2.code > 0);

create table sl(a int, b int, c int);

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
  tenk1 t1 join tenk2 t2 on t1.two = t2.two; -- error, prefer "t1" suggestion

select t2.uunique1 from
  tenk1 t1 join tenk2 t2 on t1.two = t2.two; -- error, prefer "t2" suggestion

select uunique1 from
  tenk1 t1 join tenk2 t2 on t1.two = t2.two; -- error, suggest both at once

select ctid from
  tenk1 t1 join tenk2 t2 on t1.two = t2.two; -- error, need qualification

select atts.relid::regclass, s.* from pg_stats s join
    pg_attribute a on s.attname = a.attname and s.tablename =
    a.attrelid::regclass::text join (select unnest(indkey) attnum,
    indexrelid from pg_index i) atts on atts.attnum = a.attnum where
    schemaname != 'pg_catalog';

SELECT * FROM (int8_tbl i cross join int4_tbl j) ss(a,b,c,d);

select f1,g from int4_tbl a, (select a.f1 as g) ss;

select f1,g from int4_tbl a cross join (select f1 as g) ss;

select f1,g from int4_tbl a cross join (select a.f1 as g) ss;

create temp table xx1 as select f1 as x1, -f1 as x2 from int4_tbl;

delete from xx1 using (select * from int4_tbl where f1 = xx1.x1) ss;

create table join_pt1p2 partition of join_pt1 for values from (100) to (200);

create table join_pt1p1p1 partition of join_pt1p1 for values from (0) to (100);

insert into join_pt1 values (1, 1, 'x'), (101, 101, 'y');

create table join_ut1 (a int, b int, c varchar);

insert into join_ut1 values (101, 101, 'y'), (2, 2, 'z');

drop table join_pt1;

drop table join_ut1;

begin;

create table fkest (x integer, x10 integer, x10b integer, x100 integer);

insert into fkest select x, x/10, x/10, x/100 from generate_series(1,1000) x;

create unique index on fkest(x, x10, x100);

analyze fkest;

alter table fkest add constraint fk
  foreign key (x, x10b, x100) references fkest (x, x10, x100);

rollback;

begin;

create table fkest (a int, b int, c int unique, primary key(a,b));

create table fkest1 (a int, b int, primary key(a,b));

insert into fkest select x/10, x%10, x from generate_series(1,1000) x;

insert into fkest1 select x/10, x%10 from generate_series(1,1000) x;

alter table fkest1
  add constraint fkest1_a_b_fkey foreign key (a,b) references fkest;

analyze fkest;

analyze fkest1;

rollback;

create table j1 (id int primary key);

create table j2 (id int primary key);

create table j3 (id int);

insert into j1 values(1),(2),(3);

insert into j2 values(1),(2),(3);

insert into j3 values(1),(1);

analyze j1;

analyze j2;

analyze j3;

drop table j1;

drop table j2;

drop table j3;

create table j1 (id1 int, id2 int, primary key(id1,id2));

create table j2 (id1 int, id2 int, primary key(id1,id2));

create table j3 (id1 int, id2 int, primary key(id1,id2));

insert into j1 values(1,1),(1,2);

insert into j2 values(1,1);

insert into j3 values(1,1);

analyze j1;

analyze j2;

analyze j3;

create unique index j1_id2_idx on j1(id2) where id2 > 0;

drop index j1_id2_idx;

set enable_nestloop to 0;

set enable_hashjoin to 0;

set enable_sort to 0;

create index j1_id1_idx on j1 (id1) where id1 % 1000 = 1;

create index j2_id1_idx on j2 (id1) where id1 % 1000 = 1;

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

drop table j1;

drop table j2;

drop table j3;

create table j3 as select unique1, tenthous from onek;

vacuum analyze j3;

create unique index on j3(unique1, tenthous);

drop table j3;

CREATE TEMP TABLE skip_fetch (a INT, b INT) WITH (fillfactor=10);

INSERT INTO skip_fetch SELECT i % 3, i FROM generate_series(0,30) i;

CREATE INDEX ON skip_fetch(a);

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

CREATE INDEX ON rescan_bhs (a);

SELECT * FROM rescan_bhs t1 LEFT JOIN rescan_bhs t2 ON t1.a IN
  (SELECT a FROM rescan_bhs t3 WHERE t2.a > 1);

RESET enable_seqscan;

RESET enable_indexscan;

CREATE TABLE group_tbl (a INT, b INT);

INSERT INTO group_tbl SELECT 1, 1;

CREATE STATISTICS group_tbl_stat (ndistinct) ON a, b FROM group_tbl;

ANALYZE group_tbl;

DROP TABLE group_tbl;

SELECT t1.unique1 FROM tenk1 t1 LEFT JOIN
  (SELECT *, 42 AS phv FROM tenk1 t2) ss ON t1.unique2 = ss.unique2
WHERE ss.unique1 = ss.phv AND t1.unique1 < 100;

SELECT COUNT(*) FROM tenk1 t1, tenk1 t2
WHERE t2.thousand = t1.tenthous OR t2.thousand = t1.unique1 OR t2.thousand = t1.unique2;

SELECT COUNT(*) FROM onek t1 LEFT JOIN tenk1 t2
    ON (t2.thousand = t1.tenthous OR t2.thousand = t1.thousand);

