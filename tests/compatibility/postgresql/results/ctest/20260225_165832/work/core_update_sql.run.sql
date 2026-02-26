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

CREATE TABLE upsert_test_1 PARTITION OF upsert_test FOR VALUES IN (1);

CREATE TABLE upsert_test_2 (b TEXT, a INT PRIMARY KEY);

INSERT INTO upsert_test VALUES(1, 'Boo'), (2, 'Zoo');

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

ALTER TABLE range_parted ENABLE ROW LEVEL SECURITY;

CREATE USER regress_range_parted_user;

GRANT ALL ON range_parted, mintab TO regress_range_parted_user;

CREATE POLICY seeall ON range_parted AS PERMISSIVE FOR SELECT USING (true);

CREATE POLICY policy_range_parted ON range_parted for UPDATE USING (true) WITH CHECK (c % 2 = 0);

SET SESSION AUTHORIZATION regress_range_parted_user;

RESET SESSION AUTHORIZATION;

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

CREATE TABLE IF NOT EXISTS part_def partition of range_parted default;

insert into range_parted values ('c', 9);

update part_def set a = 'd' where a = 'c';

UPDATE range_parted set a = 'ad' WHERE a = 'a';

UPDATE range_parted set a = 'bd' WHERE a = 'b';

UPDATE range_parted set a = 'a' WHERE a = 'ad';

UPDATE range_parted set a = 'b' WHERE a = 'bd';

CREATE TABLE list_part1  PARTITION OF list_parted for VALUES in ('a', 'b');

CREATE TABLE list_default PARTITION OF list_parted default;

INSERT into list_part1 VALUES ('a', 1);

INSERT into list_default VALUES ('d', 10);

UPDATE list_default set a = 'x' WHERE a = 'd';

CREATE TABLE IF NOT EXISTS utr1 (a int check (a in (1)), q text, b text);

CREATE TABLE IF NOT EXISTS utr2 (a int check (a in (2)), b text);

alter table utr1 drop column q;

CREATE TABLE sub_part1(b int, c int8, a numeric);

CREATE TABLE sub_part2(b int, c int8, a numeric);

CREATE TABLE list_part1(a numeric, b int, c int8);

INSERT into list_parted VALUES (2,5,50);

INSERT into list_parted VALUES (3,6,60);

INSERT into sub_parted VALUES (1,1,60);

INSERT into sub_parted VALUES (1,2,10);

UPDATE sub_parted set a = 2 WHERE c = 10;

UPDATE list_parted set b = c + a WHERE a = 2;

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

CREATE TABLE non_parted (id int);

INSERT into non_parted VALUES (1), (1), (1), (2), (2), (2), (3), (3), (3);

UPDATE list_parted t1 set a = 2 FROM non_parted t2 WHERE t1.a = t2.id and a = 1;

create or replace function dummy_hashint4(a int4, seed int8) returns int8 as
$$ begin return (a + seed); end; $$ language 'plpgsql' immutable;

create operator class custom_opclass for type int4 using hash as
operator 1 = , function 2 dummy_hashint4(int4, int8);

CREATE TABLE IF NOT EXISTS hpart1 partition of hash_parted for values with (modulus 2, remainder 1);

CREATE TABLE IF NOT EXISTS hpart2 partition of hash_parted for values with (modulus 4, remainder 2);

CREATE TABLE IF NOT EXISTS hpart3 partition of hash_parted for values with (modulus 8, remainder 0);

CREATE TABLE IF NOT EXISTS hpart4 partition of hash_parted for values with (modulus 8, remainder 4);

insert into hpart1 values (1, 1);

insert into hpart2 values (2, 5);

insert into hpart4 values (3, 4);

update hash_parted set b = b - 1 where b = 1;

update hash_parted set b = b + 8 where b = 1;

drop operator class custom_opclass using hash;

