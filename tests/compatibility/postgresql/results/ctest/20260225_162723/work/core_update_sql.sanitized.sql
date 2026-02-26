CREATE TABLE update_test (
    a   INT DEFAULT 10,
    b   INT,
    c   TEXT
);

CREATE TABLE upsert_test (
    a   INT PRIMARY KEY,
    b   TEXT
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

INSERT INTO upsert_test VALUES(1, 'Boo'), (3, 'Zoo');

WITH aaa AS (SELECT 1 AS a, 'Foo' AS b) INSERT INTO upsert_test
  VALUES (1, 'Bar') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT b, a FROM aaa) RETURNING *;

INSERT INTO upsert_test VALUES (1, 'Baz'), (3, 'Zaz') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT b || ', Correlated', a from upsert_test i WHERE i.a = upsert_test.a)
  RETURNING *;

INSERT INTO upsert_test VALUES (1, 'Bat'), (3, 'Zot') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT b || ', Excluded', a from upsert_test i WHERE i.a = excluded.a)
  RETURNING *;

DROP TABLE update_test;

DROP TABLE upsert_test;

CREATE TABLE upsert_test_1 PARTITION OF upsert_test FOR VALUES IN (1);

CREATE TABLE upsert_test_2 (b TEXT, a INT PRIMARY KEY);

INSERT INTO upsert_test VALUES(1, 'Boo'), (2, 'Zoo');

WITH aaa AS (SELECT 1 AS a, 'Foo' AS b) INSERT INTO upsert_test
  VALUES (1, 'Bar') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT b, a FROM aaa) RETURNING *;

WITH aaa AS (SELECT 1 AS ctea, ' Foo' AS cteb) INSERT INTO upsert_test
  VALUES (1, 'Bar'), (2, 'Baz') ON CONFLICT(a)
  DO UPDATE SET (b, a) = (SELECT upsert_test.b||cteb, upsert_test.a FROM aaa) RETURNING *;

DROP TABLE upsert_test;

CREATE TABLE part_b_20_b_30 (e varchar, c numeric, a text, b bigint, d int);

CREATE TABLE part_b_1_b_10 PARTITION OF range_parted FOR VALUES FROM ('b', 1) TO ('b', 10);

CREATE TABLE part_a_10_a_20 PARTITION OF range_parted FOR VALUES FROM ('a', 10) TO ('a', 20);

CREATE TABLE part_a_1_a_10 PARTITION OF range_parted FOR VALUES FROM ('a', 1) TO ('a', 10);

UPDATE part_b_10_b_20 set b = b - 6;

ALTER TABLE part_c_100_200 DROP COLUMN e, DROP COLUMN c, DROP COLUMN a;

ALTER TABLE part_c_100_200 ADD COLUMN c numeric, ADD COLUMN e varchar, ADD COLUMN a text;

ALTER TABLE part_c_100_200 DROP COLUMN b;

ALTER TABLE part_c_100_200 ADD COLUMN b bigint;

CREATE TABLE part_d_1_15 PARTITION OF part_c_100_200 FOR VALUES FROM (1) TO (15);

CREATE TABLE part_d_15_20 PARTITION OF part_c_100_200 FOR VALUES FROM (15) TO (20);

CREATE TABLE part_c_1_100 (e varchar, d int, c numeric, b bigint, a text);

UPDATE range_parted set d = d - 10 WHERE d > 10;

UPDATE range_parted set e = d;

UPDATE part_c_1_100 set c = c + 20 WHERE c = 98;

UPDATE part_b_10_b_20 set c = c + 20 returning c, b, a;

UPDATE range_parted set b = b - 6 WHERE c > 116 returning a, b + c;

CREATE TABLE mintab(c1 int);

INSERT into mintab VALUES (120);

CREATE VIEW upview AS SELECT * FROM range_parted WHERE (select c > c1 FROM mintab) WITH CHECK OPTION;

UPDATE upview set c = 199 WHERE b = 4;

UPDATE upview set a = 'b', b = 15 WHERE b = 4;

DROP VIEW upview;

UPDATE range_parted set c = 95 WHERE a = 'b' and b > 10 and c > 100 returning (range_parted), *;

CREATE FUNCTION trans_updatetrigfunc() RETURNS trigger LANGUAGE plpgsql AS
$$
  begin
    raise notice 'trigger = %, old table = %, new table = %',
                 TG_NAME,
                 (select string_agg(old_table::text, ', ' ORDER BY a) FROM old_table),
                 (select string_agg(new_table::text, ', ' ORDER BY a) FROM new_table);

return null;

end;

$$;

CREATE TRIGGER trans_updatetrig
  AFTER UPDATE ON range_parted REFERENCING OLD TABLE AS old_table NEW TABLE AS new_table
  FOR EACH STATEMENT EXECUTE PROCEDURE trans_updatetrigfunc();

UPDATE range_parted set c = (case when c = 96 then 110 else c + 1 end ) WHERE a = 'b' and b > 10 and c >= 96;

CREATE TRIGGER trans_deletetrig
  AFTER DELETE ON range_parted REFERENCING OLD TABLE AS old_table
  FOR EACH STATEMENT EXECUTE PROCEDURE trans_updatetrigfunc();

CREATE TRIGGER trans_inserttrig
  AFTER INSERT ON range_parted REFERENCING NEW TABLE AS new_table
  FOR EACH STATEMENT EXECUTE PROCEDURE trans_updatetrigfunc();

UPDATE range_parted set c = c + 50 WHERE a = 'b' and b > 10 and c >= 96;

DROP TRIGGER trans_deletetrig ON range_parted;

DROP TRIGGER trans_inserttrig ON range_parted;

CREATE FUNCTION func_parted_mod_b() RETURNS trigger AS $$
BEGIN
   NEW.b = NEW.b + 1;

return NEW;

END $$ language plpgsql;

CREATE TRIGGER trig_c1_100 BEFORE UPDATE OR INSERT ON part_c_1_100
   FOR EACH ROW EXECUTE PROCEDURE func_parted_mod_b();

CREATE TRIGGER trig_d1_15 BEFORE UPDATE OR INSERT ON part_d_1_15
   FOR EACH ROW EXECUTE PROCEDURE func_parted_mod_b();

CREATE TRIGGER trig_d15_20 BEFORE UPDATE OR INSERT ON part_d_15_20
   FOR EACH ROW EXECUTE PROCEDURE func_parted_mod_b();

UPDATE range_parted set c = (case when c = 96 then 110 else c + 1 end) WHERE a = 'b' and b > 10 and c >= 96;

UPDATE range_parted set c = c + 50 WHERE a = 'b' and b > 10 and c >= 96;

UPDATE range_parted set b = 15 WHERE b = 1;

DROP TRIGGER trans_updatetrig ON range_parted;

DROP TRIGGER trig_c1_100 ON part_c_1_100;

DROP TRIGGER trig_d1_15 ON part_d_1_15;

DROP TRIGGER trig_d15_20 ON part_d_15_20;

DROP FUNCTION func_parted_mod_b();

ALTER TABLE range_parted ENABLE ROW LEVEL SECURITY;

CREATE USER regress_range_parted_user;

GRANT ALL ON range_parted, mintab TO regress_range_parted_user;

CREATE POLICY seeall ON range_parted AS PERMISSIVE FOR SELECT USING (true);

CREATE POLICY policy_range_parted ON range_parted for UPDATE USING (true) WITH CHECK (c % 2 = 0);

SET SESSION AUTHORIZATION regress_range_parted_user;

RESET SESSION AUTHORIZATION;

CREATE FUNCTION func_d_1_15() RETURNS trigger AS $$
BEGIN
   NEW.c = NEW.c + 1; -- Make even numbers odd, or vice versa

return NEW;

END $$ LANGUAGE plpgsql;

CREATE TRIGGER trig_d_1_15 BEFORE INSERT ON part_d_1_15
   FOR EACH ROW EXECUTE PROCEDURE func_d_1_15();

SET SESSION AUTHORIZATION regress_range_parted_user;

UPDATE range_parted set a = 'b', c = 151 WHERE a = 'a' and c = 200;

RESET SESSION AUTHORIZATION;

SET SESSION AUTHORIZATION regress_range_parted_user;

RESET SESSION AUTHORIZATION;

DROP TRIGGER trig_d_1_15 ON part_d_1_15;

DROP FUNCTION func_d_1_15();

RESET SESSION AUTHORIZATION;

CREATE POLICY policy_range_parted_subplan on range_parted
    AS RESTRICTIVE for UPDATE USING (true)
    WITH CHECK ((SELECT range_parted.c <= c1 FROM mintab));

SET SESSION AUTHORIZATION regress_range_parted_user;

UPDATE range_parted set a = 'b', c = 120 WHERE a = 'a' and c = 200;

RESET SESSION AUTHORIZATION;

CREATE POLICY policy_range_parted_wholerow on range_parted AS RESTRICTIVE for UPDATE USING (true)
   WITH CHECK (range_parted = row('b', 10, 112, 1, NULL)::range_parted);

SET SESSION AUTHORIZATION regress_range_parted_user;

UPDATE range_parted set a = 'b', c = 112 WHERE a = 'a' and c = 200;

RESET SESSION AUTHORIZATION;

SET SESSION AUTHORIZATION regress_range_parted_user;

RESET SESSION AUTHORIZATION;

DROP POLICY policy_range_parted ON range_parted;

DROP POLICY policy_range_parted_subplan ON range_parted;

DROP POLICY policy_range_parted_wholerow ON range_parted;

REVOKE ALL ON range_parted, mintab FROM regress_range_parted_user;

DROP USER regress_range_parted_user;

DROP TABLE mintab;

CREATE FUNCTION trigfunc() returns trigger language plpgsql as
$$
  begin
    raise notice 'trigger = % fired on table % during %',
                 TG_NAME, TG_TABLE_NAME, TG_OP;

return null;

end;

$$;

CREATE TRIGGER parent_delete_trig
  AFTER DELETE ON range_parted for each statement execute procedure trigfunc();

CREATE TRIGGER parent_update_trig
  AFTER UPDATE ON range_parted for each statement execute procedure trigfunc();

CREATE TRIGGER parent_insert_trig
  AFTER INSERT ON range_parted for each statement execute procedure trigfunc();

CREATE TRIGGER c1_delete_trig
  AFTER DELETE ON part_c_1_100 for each statement execute procedure trigfunc();

CREATE TRIGGER c1_update_trig
  AFTER UPDATE ON part_c_1_100 for each statement execute procedure trigfunc();

CREATE TRIGGER c1_insert_trig
  AFTER INSERT ON part_c_1_100 for each statement execute procedure trigfunc();

CREATE TRIGGER d1_delete_trig
  AFTER DELETE ON part_d_1_15 for each statement execute procedure trigfunc();

CREATE TRIGGER d1_update_trig
  AFTER UPDATE ON part_d_1_15 for each statement execute procedure trigfunc();

CREATE TRIGGER d1_insert_trig
  AFTER INSERT ON part_d_1_15 for each statement execute procedure trigfunc();

CREATE TRIGGER d15_delete_trig
  AFTER DELETE ON part_d_15_20 for each statement execute procedure trigfunc();

CREATE TRIGGER d15_update_trig
  AFTER UPDATE ON part_d_15_20 for each statement execute procedure trigfunc();

CREATE TRIGGER d15_insert_trig
  AFTER INSERT ON part_d_15_20 for each statement execute procedure trigfunc();

UPDATE range_parted set c = c - 50 WHERE c > 97;

DROP TRIGGER parent_delete_trig ON range_parted;

DROP TRIGGER parent_update_trig ON range_parted;

DROP TRIGGER parent_insert_trig ON range_parted;

DROP TRIGGER c1_delete_trig ON part_c_1_100;

DROP TRIGGER c1_update_trig ON part_c_1_100;

DROP TRIGGER c1_insert_trig ON part_c_1_100;

DROP TRIGGER d1_delete_trig ON part_d_1_15;

DROP TRIGGER d1_update_trig ON part_d_1_15;

DROP TRIGGER d1_insert_trig ON part_d_1_15;

DROP TRIGGER d15_delete_trig ON part_d_15_20;

DROP TRIGGER d15_update_trig ON part_d_15_20;

DROP TRIGGER d15_insert_trig ON part_d_15_20;

create table part_def partition of range_parted default;

insert into range_parted values ('c', 9);

update part_def set a = 'd' where a = 'c';

UPDATE range_parted set a = 'ad' WHERE a = 'a';

UPDATE range_parted set a = 'bd' WHERE a = 'b';

UPDATE range_parted set a = 'a' WHERE a = 'ad';

UPDATE range_parted set a = 'b' WHERE a = 'bd';

DROP TABLE range_parted;

CREATE TABLE list_part1  PARTITION OF list_parted for VALUES in ('a', 'b');

CREATE TABLE list_default PARTITION OF list_parted default;

INSERT into list_part1 VALUES ('a', 1);

INSERT into list_default VALUES ('d', 10);

UPDATE list_default set a = 'x' WHERE a = 'd';

DROP TABLE list_parted;

create table utr1 (a int check (a in (1)), q text, b text);

create table utr2 (a int check (a in (2)), b text);

alter table utr1 drop column q;

drop table utrtest;

CREATE TABLE sub_part1(b int, c int8, a numeric);

CREATE TABLE sub_part2(b int, c int8, a numeric);

CREATE TABLE list_part1(a numeric, b int, c int8);

INSERT into list_parted VALUES (2,5,50);

INSERT into list_parted VALUES (3,6,60);

INSERT into sub_parted VALUES (1,1,60);

INSERT into sub_parted VALUES (1,2,10);

UPDATE sub_parted set a = 2 WHERE c = 10;

UPDATE list_parted set b = c + a WHERE a = 2;

CREATE FUNCTION func_parted_mod_b() returns trigger as $$
BEGIN
   NEW.b = 2; -- This is changing partition key column.

return NEW;

END $$ LANGUAGE plpgsql;

CREATE TRIGGER parted_mod_b before update on sub_part1
   for each row execute procedure func_parted_mod_b();

UPDATE list_parted set c = 70 WHERE b  = 1;

DROP TRIGGER parted_mod_b ON sub_part1;

CREATE OR REPLACE FUNCTION func_parted_mod_b() returns trigger as $$
BEGIN
   raise notice 'Trigger: Got OLD row %, but returning NULL', OLD;

return NULL;

END $$ LANGUAGE plpgsql;

CREATE TRIGGER trig_skip_delete before delete on sub_part2
   for each row execute procedure func_parted_mod_b();

UPDATE list_parted set b = 1 WHERE c = 70;

DROP TRIGGER trig_skip_delete ON sub_part2;

UPDATE list_parted set b = 1 WHERE c = 70;

DROP FUNCTION func_parted_mod_b();

CREATE TABLE non_parted (id int);

INSERT into non_parted VALUES (1), (1), (1), (2), (2), (2), (3), (3), (3);

UPDATE list_parted t1 set a = 2 FROM non_parted t2 WHERE t1.a = t2.id and a = 1;

DROP TABLE non_parted;

DROP TABLE list_parted;

create or replace function dummy_hashint4(a int4, seed int8) returns int8 as
$$ begin return (a + seed); end; $$ language 'plpgsql' immutable;

create operator class custom_opclass for type int4 using hash as
operator 1 = , function 2 dummy_hashint4(int4, int8);

create table hpart1 partition of hash_parted for values with (modulus 2, remainder 1);

create table hpart2 partition of hash_parted for values with (modulus 4, remainder 2);

create table hpart3 partition of hash_parted for values with (modulus 8, remainder 0);

create table hpart4 partition of hash_parted for values with (modulus 8, remainder 4);

insert into hpart1 values (1, 1);

insert into hpart2 values (2, 5);

insert into hpart4 values (3, 4);

update hash_parted set b = b - 1 where b = 1;

update hash_parted set b = b + 8 where b = 1;

drop table hash_parted;

drop operator class custom_opclass using hash;

drop function dummy_hashint4(a int4, seed int8);

