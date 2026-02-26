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
}else{next;
}}' onek.data | sort +0n -1
SELECT * FROM onek
   WHERE onek.unique1 < 10
   ORDER BY onek.unique1;
}else{next;
}}' onek.data | sort +0nr -1
SELECT onek.unique1, onek.stringu1 FROM onek
   WHERE onek.unique1 < 20
   ORDER BY unique1 using >;
}else{next;
}}' onek.data | sort +1d -2
SELECT onek.unique1, onek.stringu1 FROM onek
   WHERE onek.unique1 > 980
   ORDER BY stringu1 using <;
}else{next;
}}' onek.data |
SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 > 980
   ORDER BY string4 using <, unique1 using >;
}else{next;
}}' onek.data |
SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 > 980
   ORDER BY string4 using >, unique1 using <;
}else{next;
}}' onek.data |
SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 < 20
   ORDER BY unique1 using >, string4 using <;
}else{next;
}}' onek.data |
SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 < 20
   ORDER BY unique1 using <, string4 using >;
ANALYZE onek2;
SET enable_seqscan TO off;
SET enable_bitmapscan TO off;
SET enable_sort TO off;
}else{next;
}}' onek.data | sort +0n -1
SELECT onek2.* FROM onek2 WHERE onek2.unique1 < 10;
}else{next;
}}' onek.data | sort +0nr -1
SELECT onek2.unique1, onek2.stringu1 FROM onek2
    WHERE onek2.unique1 < 20
    ORDER BY unique1 using >;
}else{next;
}}' onek.data | sort +1d -2
SELECT onek2.unique1, onek2.stringu1 FROM onek2
   WHERE onek2.unique1 > 980;
RESET enable_seqscan;
RESET enable_bitmapscan;
RESET enable_sort;
}' person.data |;
}else{print;
}}' - emp.data |;
}else{print;
}}' - student.data |;
}{if(NF!=2){print $4,$5;
}else{print;
}}' - stud_emp.data;
}' person.data |;
}else{print;
}}' - emp.data |;
}else{print;
}}' - student.data |;
}{if(NF!=1){print $4,$5;
}else{print;
select foo from (select 1 offset 0) as foo;
select foo from (select null offset 0) as foo;
select foo from (select 'xyzzy',1,null offset 0) as foo;
select * from onek, (values(147, 'RFAAAA'), (931, 'VJAAAA')) as v (i, j)
    WHERE onek.unique1 = v.i and onek.stringu1 = v.j;
select * from onek,
  (values ((select i from
    (values(10000), (2), (389), (1000), (2000), ((select 10029))) as foo(i)
    order by i asc limit 1))) bar (i)
  where onek.unique1 = bar.i;
select * from onek
    where (unique1,ten) in (values (1,1), (20,0), (99,9), (17,99))
    order by unique1;
VALUES (1,2), (3,4+4), (7,77.7);
VALUES (1,2), (3,4+4), (7,77.7)
UNION ALL
SELECT 2+2, 57
UNION ALL
TABLE int8_tbl;
CREATE TEMP TABLE nocols();
INSERT INTO nocols DEFAULT VALUES;
SELECT * FROM nocols n, LATERAL (VALUES(n.*)) v;
CREATE TEMP TABLE foo (f1 int);
INSERT INTO foo VALUES (42),(3),(10),(7),(null),(null),(1);
SELECT * FROM foo ORDER BY f1;
SELECT * FROM foo ORDER BY f1 ASC;
SELECT * FROM foo ORDER BY f1 NULLS FIRST;
SELECT * FROM foo ORDER BY f1 DESC;
SELECT * FROM foo ORDER BY f1 DESC NULLS LAST;
CREATE INDEX fooi ON foo (f1);
SET enable_sort = false;
SELECT * FROM foo ORDER BY f1;
SELECT * FROM foo ORDER BY f1 NULLS FIRST;
SELECT * FROM foo ORDER BY f1 DESC;
SELECT * FROM foo ORDER BY f1 DESC NULLS LAST;
DROP INDEX fooi;
CREATE INDEX fooi ON foo (f1 DESC);
SELECT * FROM foo ORDER BY f1;
SELECT * FROM foo ORDER BY f1 NULLS FIRST;
SELECT * FROM foo ORDER BY f1 DESC;
SELECT * FROM foo ORDER BY f1 DESC NULLS LAST;
DROP INDEX fooi;
CREATE INDEX fooi ON foo (f1 DESC NULLS LAST);
SELECT * FROM foo ORDER BY f1;
SELECT * FROM foo ORDER BY f1 NULLS FIRST;
SELECT * FROM foo ORDER BY f1 DESC;
SELECT * FROM foo ORDER BY f1 DESC NULLS LAST;
select * from onek2 where unique2 = 11 and stringu1 = 'ATAAAA';
select unique2 from onek2 where unique2 = 11 and stringu1 = 'ATAAAA';
select * from onek2 where unique2 = 11 and stringu1 < 'B';
select unique2 from onek2 where unique2 = 11 and stringu1 < 'B';
select unique2 from onek2 where unique2 = 11 and stringu1 < 'B' for update;
select unique2 from onek2 where unique2 = 11 and stringu1 < 'C';
SET enable_indexscan TO off;
select unique2 from onek2 where unique2 = 11 and stringu1 < 'B';
RESET enable_indexscan;
select unique1, unique2 from onek2
  where (unique2 = 11 or unique1 = 0) and stringu1 < 'B';
select unique1, unique2 from onek2
  where (unique2 = 11 and stringu1 < 'B') or unique1 = 0;
SELECT 1 AS x ORDER BY x;
create function sillysrf(int) returns setof int as
  'values (1),(10),(2),($1)' language sql immutable;
select sillysrf(42);
select sillysrf(-1) order by 1;
drop function sillysrf(int);
select * from (values (2),(null),(1)) v(k) where k = k order by k;
select * from (values (2),(null),(1)) v(k) where k = k;
drop table list_parted_tbl;
