SELECT * FROM onek
   WHERE onek.unique1 < 10
   ORDER BY onek.unique1;

SELECT onek.unique1, onek.stringu1 FROM onek
   WHERE onek.unique1 < 20
   ORDER BY unique1 using >;

SELECT onek.unique1, onek.stringu1 FROM onek
   WHERE onek.unique1 > 980
   ORDER BY stringu1 using <;

SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 > 980
   ORDER BY string4 using <, unique1 using >;

SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 > 980
   ORDER BY string4 using >, unique1 using <;

SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 < 20
   ORDER BY unique1 using >, string4 using <;

SELECT onek.unique1, onek.string4 FROM onek
   WHERE onek.unique1 < 20
   ORDER BY unique1 using <, string4 using >;

ANALYZE onek2;

SET enable_seqscan TO off;

SET enable_bitmapscan TO off;

SET enable_sort TO off;

SELECT onek2.* FROM onek2 WHERE onek2.unique1 < 10;

SELECT onek2.unique1, onek2.stringu1 FROM onek2
    WHERE onek2.unique1 < 20
    ORDER BY unique1 using >;

SELECT onek2.unique1, onek2.stringu1 FROM onek2
   WHERE onek2.unique1 > 980;

RESET enable_seqscan;

RESET enable_bitmapscan;

RESET enable_sort;

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

