#coding:utf-8

"""
ID:          issue-3728-basic
ISSUE:       3728
TITLE:       Cursors should ignore changes made by the same statement
DESCRIPTION:
  This test verifies BASIC issues that were accumulated in miscenaleous tickets.
  More complex cases (which involve not only SQL but also PSQL features) will
  follow in separate .fbt(s) in order to keep size of each test in reasonable limits.
NOTES:
[18.08.2020]
  FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
  statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
  gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
  See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
  This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt
  Because of this, it was decided to replace 'alter sequence restart...' with subtraction of two gen values:
    c = gen_id(<g>, -gen_id(<g>, 0)) -- see procedure sp_restart_sequences.
JIRA:        CORE-3362
FBTEST:      bugs.core_3362_basic
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_restart_sequences as begin end;
    set  term ^;
    execute block as
    begin
        begin
            execute statement 'drop sequence g1';
            when any do begin end
        end
        begin
            execute statement 'drop sequence g2';
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create sequence g1;
    create sequence g2;
    commit;

    set term ^;
    create or alter procedure sp_restart_sequences as
        declare c bigint;
    begin
        c = gen_id(g1, -gen_id(g1, 0));
        c = gen_id(g2, -gen_id(g2, 0));
    end
    ^
    set term ;^
    commit;

    set list on;


    --###########################################################################

    --  CORE-92 infinite insertion cycle

    recreate table test(x int);
    commit;
    insert into test values(0);
    commit;

    execute procedure sp_restart_sequences;
    commit;

    insert into test(x) select gen_id(g2, 1) from test where gen_id(g1, 1) < 10;
    set count on;
    select 'core-92' as test_case, t.* from test t;
    set count off;
    commit;

    /********************************
    Expected output:
    X                               0
    X                               1
    Records affected: 2
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5.x: 10 records, wrong)


    --###########################################################################

    -- CORE-634 Bad treatment of FIRST/SKIP in subselect

    execute procedure sp_restart_sequences;
    commit;

    recreate table test(x int);
    commit;

    insert into test(x) select gen_id(g1, 1) from rdb$types rows 10;
    commit;

    delete from test where x in (select first 5 x from test);
    set count on;
    select 'core-634, case-1' as test_case, t.* from test t;
    set count off;
    commit;


    /********************************
    Expected output:
    X                               6
    X                               7
    X                               8
    X                               9
    X                               10
    Records affected: 5
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5.x: 0 records selected, i.e. all rows have been deleted, wrong)


    recreate table test(id int, val char(5));
    commit;

    insert into test values(1, 'red');
    insert into test values(2, 'green');
    insert into test values(3, 'blue');
    insert into test values(2, 'green');

    delete from test
    where test.ID in (select id from test GROUP BY id HAVING count(id)>1);

    set count on;
    select 'core-634, case-2' as test_case, t.* from test t;
    set count off;
    commit;
    /********************************
    Expected output:

     ID VAL
     == =====
      1 red
      3 blue
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5.x: 3 records selected, additional one: id,val={2,green})

    --###########################################################################

    -- CORE-2799 Changing sort directon in delete statement cause deletion of all records in table

    execute procedure sp_restart_sequences;
    commit;

    recreate table test(x int);
    create index test_x_asc on test(x);
    create descending index test_x_dec on test(x);
    commit;

    insert into test(x) select gen_id(g1, 1) from rdb$types rows 5;
    commit;

    -- Trying to remove row with minimal 'x'. In 2.5 this lead to removal ALL subsequent rows.
    delete from test a where not exists(select * from test b where b.x < a.x) order by a.x asc;
    -- adding "order by a.x+0;" forces 2.5 to work fine
    set count on;
    select 'core-2799, case-1' as test_case, t.* from test t;
    set count off;

    /********************************
    Expected output:
    X                               2
    X                               3
    X                               4
    X                               5
    Records affected: 4
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5.x: 0 records selected, i.e. all rows have been deleted, wrong)

    rollback;

    -- Trying to remove row with maximal 'x'. In 2.5 this lead to removal ALL subsequent rows.
    delete from test a where not exists(select * from test b where b.x > a.x) order by a.x desc;
    set count on;
    select 'core-2799, case-2' as test_case, t.* from test t;
    set count off;
    /********************************
    Expected output:
    X                               1
    X                               2
    X                               3
    X                               4
    Records affected: 4
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5.x: 0 records selected, i.e. all rows have been deleted, wrong)

    rollback;


    --###########################################################################



    -- Add sample from ticket:
    -- #######################

    recreate table test2 (
        id integer not null primary key using index test2_id,
        kod varchar(5)
    );
    commit;
    insert into test2(id, kod) values(1, 'abc');
    insert into test2(id, kod) values(2, 'abc');
    commit;

    -- on 2.5 this will remove ALL rows from test2:
    delete from test2 t1 where exists(select * from test2 t2 where t2.id<>t1.id and t2.kod=t1.kod) order by t1.id asc;
    set count on;
    select 'core-2799, case-3' as test_case, t.* from test2 t;
    set count off;
    /********************************
    Expected output:
    Records affected: 0
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5: one record remain in table test2 with id=2)
    rollback;

    drop table test2;
    commit;


    --###########################################################################

    -- CORE-3317 Success of deleting rows depending on order of row insertion

    recreate table cls (
      id numeric(18)
        constraint cls_id_ck_c not null,
      id_parent numeric(18)
        constraint cls_idparent_ck_c not null,
      id_child numeric(18)
        constraint cls_idchild_ck_c not null,
      depth numeric(18)
        constraint cls_depth_ck_c not null,
      constraint cls_ui1_c unique(id_parent, depth, id_child),
      constraint cls_ui2_c unique(id_child, id_parent),
      constraint cls_pk_c primary key (id)
    );
    commit;

    insert into cls values(1, 2, 2, 0);
    insert into cls values(2, 7, 7, 0);
    insert into cls values(3, 10, 10, 0);
    insert into cls values(4, 2, 7, 1);
    insert into cls values(5, 2, 10, 2);
    insert into cls values(6, 7, 10, 1);
    commit;

    delete
    from cls
    where id in (
        select c.id
        from cls c
        join cls p on c.id_parent = p.id_parent
        where
            p.id_child = 7
            and p.depth = 1
            and c.depth >= 1
    );

    set count on;
    select 'core-3317' as test_case, c.* from cls c order by id;
    set count off;

    /********************************
    Expected output:
    ID                              1
    ID_PARENT                       2
    ID_CHILD                        2
    DEPTH                           0

    ID                              2
    ID_PARENT                       7
    ID_CHILD                        7
    DEPTH                           0

    ID                              3
    ID_PARENT                       10
    ID_CHILD                        10
    DEPTH                           0

    ID                              6
    ID_PARENT                       7
    ID_CHILD                        10
    DEPTH                           1

    Records affected: 4
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (compared with PG 9.4);  2.5.x: "records affected: 5", wrong

    rollback;

    drop table cls;


    --###########################################################################

    -- CORE-3362. Cursors should ignore changes made by the same statement

    -- Issues of 18.10.2011 08:49.

    -- Case-1.
    -- ~~~~~~

    recreate table t(id int primary key, f01 int);
    commit;
    insert into t values(1, 100);
    insert into t values(2, 200);
    insert into t values(3, 300);
    commit;

    update t set f01=(select sum(f01) from t);

    set count on;
    select 'core-3362, case-1' as test_case, t.* from t order by id;
    set count off;

    /********************************
    Expected output:
    ID                              1
    F01                             600

    ID                              2
    F01                             600

    ID                              3
    F01                             600

    Records affected: 3
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5.x: f01={600, 1100, 2000} - wrong)

    rollback;

    drop table t;
    commit;


    --###########################################################################


    -- Case-2.
    -- ~~~~~~~

    recreate table t(id int primary key, f01 int);
    commit;
    -- "reverse" order of values in f01 for same IDs: from big to small
    insert into t values(1, 300);
    insert into t values(2, 200);
    insert into t values(3, 100);
    commit;

    update t set f01=(select sum(f01) from t);

    set count on;
    select 'core-3362, case-2' as test_case, t.* from t;
    set count off;
    rollback;

    drop table t;
    commit;

    /********************************
    Expected output:
    ID                              1
    F01                             600

    ID                              2
    F01                             600

    ID                              3
    F01                             600

    Records affected: 3
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 (2.5.x: f01={600, 900, 1600} - wrong)


    --###########################################################################


    -- Issue of 03.01.2014 05:07.
    recreate table t(
        i int,
        x int, y int,
        constraint t_pk primary key(x),
        constraint t_fk foreign key(y) references t(x) on update cascade -- SELF-REFERENCING
    );
    commit;

    insert into t(x, y) values(1, null);
    insert into t(x, y) values(2, 1 );
    insert into t(x, y) values(3, 2 );
    insert into t(x, y) values(4, 3 );
    insert into t(x, y) values(5, 4 );
    update t set y=5 where x=1; -- "closure" of chain
    update t set i=x; -- backup initial values of 'x'
    commit;
    -- select * from t;

    update t set x=y+1;

    set count on;
    select 'core-3362, case-3' as test_case, t.* from t order by i;
    set count off;

    /********************************
    Expected output (compared with PG 9.4)
     I            X            Y
    == ============ ============
     1            6            5
     2            2            6
     3            3            2
     4            4            3
     5            5            4
    Records affected: 5
    *********************************/
    -- Result: OK on WI-T4.0.0.371, WI-V3.0.1.32597 2.5.x - wrong data at all.
    -- 3.0.0.32030 = FAIL <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  F A I L <<<<<<<<<<<<<<
    -- SQLSTATE = 23000
    -- violation of FOREIGN KEY constraint "T_FK" on table "T"
    -- -Foreign key reference target does not exist
    -- -unknown ISC error 335545072
    -- 3.0.0.31896 = FAIL (Beta-2 release)
    -- 3.0.0.31374 = OK (Beta-1 release)

    rollback;

    drop table t;
    commit;


    --###########################################################################



    -- Issues of 03.01.2014 05:13.
    -- ~~~~~~~~~~~~~~~~~~~~~~~~~~~
    -- See also: http://www.sql.ru/forum/1068804/rezultat-count-f1-over-v-processe-update-set-f1-null-cursor-stability?hl=
    -- Case-1.
    -- ~~~~~~~

    recreate table t(x int, y int);
    commit;
    insert into t values(1, 1);
    insert into t values(2, 2);
    insert into t values(3, 3);
    insert into t values(4, 4);
    insert into t values(5, 5);
    commit;

    update t set x=null, y=(select c from (select count(x)over() c from t) rows 1);

    set count on;
    select 'core-3362, case-4' as test_case, t.* from t;
    set count off;
    rollback;

    drop table t;
    commit;

    /********************************
    Expected output
    X                               <null>
    Y                               5

    X                               <null>
    Y                               5

    X                               <null>
    Y                               5

    X                               <null>
    Y                               5

    X                               <null>
    Y                               5
    Records affected: 5
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597; 2.5.x - can't be checked because of OVER() clause.


    --###########################################################################


    -- Case-2.
    -- ~~~~~~~~~

    recreate table t(x int, y int);
    commit;
    insert into t values(1, 1);
    insert into t values(2, 2);
    insert into t values(3, 3);
    insert into t values(4, 4);
    insert into t values(5, 5);
    commit;

    update t
        set y=(select (select sum(x) from t tx where tx.x<=tt.x)
                 from t tt
                 order by y desc rows 1
              );

    set count on;
    select 'core-3362, case-5' as test_case, t.* from t;
    set count off;

    /********************************
    Expected output:
    X                               1
    Y                               15

    X                               2
    Y                               15

    X                               3
    Y                               15

    X                               4
    Y                               15

    X                               5
    Y                               15

    Records affected: 5
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597; 2.5.x: wrong data in t.y: {15,1,1,1}

    rollback;

    drop table t;
    commit;



    --###########################################################################


    -- Additional issue: sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1068804&msg=15378766 (05.01.2014)
    recreate table t(x int primary key, y int);
    commit;
    insert into t values(5,5);
    insert into t values(4,5);
    insert into t values(3,2);
    insert into t values(2,2);
    insert into t values(1,0);
    commit;

    delete from t m where (select count(*)over() from t x where x.y=m.y rows 1) > 1;

    set count on;
    select 'core-3362, case-6' as test_case, t.* from t;
    set count off;
    rollback;

    /********************************
    Expected output:
    X                               1
    Y                               0

    Records affected: 1
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597; 2.5.x: can't be tested because of OVER() clause.

    drop table t;
    commit;


    --###########################################################################



    -- Issue 01.03.2014 02:22

    recreate table t(id int primary key, x int);
    commit;
    insert into t(id, x) select row_number()over(), row_number()over() from rdb$types rows 3;
    commit;
    update t set x=null where x not in(select x from t where x is null);

    set count on;
    select 'core-3362, case-7' as test_case, t.* from t;
    set count off;
    rollback;

    /********************************
    Expected output:
    ID                              1
    X                               <null>

    ID                              2
    X                               <null>

    ID                              3
    X                               <null>

    Records affected: 3
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597; 2.5.x: can't be tested because of OVER() clause.

    drop table t;
    commit;



    --###########################################################################



    -- Issue 12.03.2014 04:56

    recreate table t(x int, y int);
    insert into t values(1, 100);
    insert into t values(2, 200);
    insert into t values(3, 300);
    commit;
    create view v as select y,count(*) cnt from t group by y;
    commit;

    update t set y=null where 2 not in( select cnt from v );

    set count on;
    select 'core-3362, case-8.1' as test_case, t.* from t;
    set count off;


    /********************************
    Expected output:
    ID                              1
    X                               <null>

    ID                              2
    X                               <null>

    ID                              3
    X                               <null>

    Records affected: 3
    *********************************/
    -- result: OK on WI-T4.0.0.371, WI-V3.0.1.32597; 2.5.x: wrong data in t.x: {null, null, 300}

    rollback;

    drop view v;
    drop table t;
    commit;


    --###########################################################################

    -- 25.09.2016: case with NOT IN and ALL predicates, see letters to hvlad, dimitr from this date:
    recreate table t(id int, x int);
    commit;

    insert into t(id, x) values(1,10);
    insert into t(id, x) values(2,20);
    insert into t(id, x) values(3,30);
    insert into t(id, x) values(4,40);
    commit;

    update t s set x = null where x not in ( select x from t z where z.x > all(select x from t y where y.id<>z.id ) );
    set count on;
    select 'core-3362, case-8.2' as test_case, t.* from t;
    set count off;
    /********************************
    Expected output:
    ID                              1
    X                               <null>

    ID                              2
    X                               <null>

    ID                              3
    X                               <null>

    ID                              4
    X                               40

    Records affected: 4
    *********************************/
    -- result: OK on 3.x, 4.0;  2.5.x: wrong for id=4: {4,null}

    rollback;

    -- :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    -- ::::::::::::::::::::::::::::::       M  E  R  G  E      :::::::::::::::::::::::::
    -- :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

    -- Issue 16.03.2014 09:51
    -- Also: http://www.sql.ru/forum/1082847/merge-ne-sovpadaut-rezultaty-fb-i-oracle-again-cursor-stability?hl=

    recreate table t(x int, y int);

    insert into t values(1, 5);
    insert into t values(2, 4);
    insert into t values(3, 3);
    insert into t values(4, 2);
    insert into t values(5, 1);

    recreate table t2(x int, y int);

    insert into t2 values(2, 4);
    insert into t2 values(5, 1);
    insert into t2 values(1, 5);
    insert into t2 values(4, 2);
    insert into t2 values(3, 3);

    -- This is only for illustrate required result: all rows should contain
    -- new value y = 4
    -- update t set t.y = (select count(*) from t where x<>y)
    -- where exists( select * from t2 s where t.x=s.y)
    -- ;
    -- rollback;

    merge into t using(select x,y from t2) s on t.x=s.y
    when matched
      then update set t.y=(select count(*) from t where x<>y);

    --set list off;
    set count on;
    select 'core-3362, case-9' as test_case, t.* from t order by x;
    set count off;
    rollback;

    /********************************
    Expected output:
    X                               1
    Y                               4

    X                               2
    Y                               4

    X                               3
    Y                               4

    X                               4
    Y                               4

    X                               5
    Y                               4
    Records affected: 5
    *********************************/

    -- Result:
    -- OK on WI-T4.0.0.371, WI-V3.0.1.32597; 2.5.x: wrong data in t.y: {4,4,4,5,5}
    -- 3.0.0.32030 = FAIL <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  F A I L <<<<<<<<<<<<<<
    -- 3.0.0.32362 = OK. // confirmed on: WI-V3.0.0.32362, 26.02.2016

    drop table t;
    drop table t2;
    commit;


    --###########################################################################



    -- Issue 14.05.2015 10:54

    recreate table test(
        id int constraint test_pk_id primary key using index test_pk_id,
        pid int constraint test_fk_pid2id references test(id)
        on update SET NULL
    );
    commit;
    insert into test values( 5, null );
    insert into test values( 4, 5 );
    insert into test values( 3, 4 );
    insert into test values( 2, 3 );
    insert into test values( 1, 2 );
    commit;
    update test set pid=1 where id=5;
    commit;

    /*
    content of table test now is:
    ID	PID
    5	1
    4	5
    3	4
    2	3
    1	2
    */
    update test set id = id + 1 order by id desc;

    set count on;
    select 'core-3362, case-10' as test_case, t.* from test t order by id desc;
    set count off;
    rollback;

    drop table test;
    commit;

    /*******************************

    EXPECTED result:

      ID          PID
    ==== ============
       6       <null>
       5       <null>
       4       <null>
       3       <null>
       2       <null>

    -- OK on  WI-T4.0.0.371, WI-V3.0.1.32597
    -- By the way: PG 9.4 (update test set id = id + 1):
    -- ERROR:  duplicate key value violates unique constraint "test_pk_id"
    -- DETAIL:  Key (id)=(5) already exists.
    **********************************/
    -- OK on WI-T4.0.0.371, WI-V3.0.1.32597


    --###########################################################################

    -- Issue 13.09.2014 09:33


    recreate table t1(id int);
    recreate table t2(id int, x int);
    commit;
    insert into t1 values(1);
    insert into t1 values(2);
    insert into t1 values(3);
    commit;

    merge into t2 using t1 on t1.id=t2.id when not matched then insert (id, x) values(t1.id, (select sum(id) from t2) );
    select 'core-3362, case-11a' as test_case, a.* from t2 a;
    rollback;

    merge into t2 using t1 on t1.id=t2.id when not matched then insert (id, x) values(t1.id, (select min(id) from t2) );
    select 'core-3362, case-11b' as test_case, a.* from t2 a;
    rollback;

    merge into t2 using t1 on t1.id=t2.id when not matched then insert (id, x) values(t1.id, (select max(id) from t2) );
    select 'core-3362, case-11c' as test_case, a.* from t2 a;
    rollback;

    merge into t2 using t1 on t1.id=t2.id when not matched then insert (id, x) values(t1.id, (select count(*) from t2) );
    select 'core-3362, case-11d' as test_case, a.* from t2 a;
    rollback;

    /*
    Expected result:
              ID            X
    ============ ============
               1       <null>
               2       <null>
               3       <null>

              ID            X
    ============ ============
               1       <null>
               2       <null>
               3       <null>

              ID            X
    ============ ============
               1       <null>
               2       <null>
               3       <null>

              ID            X
    ============ ============
               1            0
               2            0
               3            0
    */
    -- OK on WI-T4.0.0.371, WI-V3.0.1.32597; totally wrong on 2.5.x



    --###########################################################################

    -- 25.09.2016: case with NOT IN and ALL predicates, see letters to hvlad, dimitr from this date:

    recreate table t1(id int, x int);
    --recreate view v_t1_checked as select * from t1 where true with check option; -- temply (?) added 07.06.2020 because of fixed core-2274

    -- 08.06.2020: 'WITH CHECK OPTION' no more helps.
    -- We have to create 'truly-updatable' view in order to avoid
    -- Statement failed, SQLSTATE = 21000
    -- Multiple source records cannot match the same target during MERGE
    recreate view v_t1_updatable as select * from t1;
    set term ^;
    create or alter trigger v_t1_biud for v_t1_updatable active before insert or update or delete position 0 as
        declare v_old_id int;
        declare v_old_x int;
        declare v_new_id int;
        declare v_new_x int;
    begin
        if (not deleting) then
            begin
                if (inserting) then
                    insert into t1(id, x) values(new.id, new.x);
                else
                    update t1 set id = new.id, x = new.x where id = old.id;
            end
        else
            begin
                delete from t1 where id = old.id;
            end
    end
    ^
    set term ;^
    commit;

    insert into v_t1_updatable(id, x) values(1,10);
    insert into v_t1_updatable(id, x) values(2,20);
    insert into v_t1_updatable(id, x) values(3,30);
    insert into v_t1_updatable(id, x) values(4,40);
    commit;


    merge into v_t1_updatable t
    using t1 s on
    t.x not in ( select x from t1 z where z.x is null or z.x > all(select x from t1 y where y.id<>z.id ) )
    when matched then update set x = null
    ;

    select 'core-3362, case-12a' as test_case, a.* from t1 a;

    /* Expected result:
        ID                              1
        X                               <null>

        ID                              2
        X                               <null>

        ID                              3
        X                               <null>

        ID                              4
        X                               40
    */
    -- OK on 3.x, wrong data on 2.5.x for id >= 2;

    rollback;


    merge into v_t1_updatable t
    using t1 s on
    t.x not in ( select x from t1 z where z.x is null or z.x > all(select x from t1 y where y.id<>z.id ) )
    when matched then update set x = null
    ;

    select 'core-3362, case-12b' as test_case, a.* from t1 a;
    /* Expected result:
        ID                              1
        X                               <null>

        ID                              2
        X                               <null>

        ID                              3
        X                               <null>

        ID                              4
        X                               40
    */
    -- OK on 3.x, wrong data on 2.5.x for id >= 2;

    rollback;


    --###########################################################################

    -- 24.10.2016 from CORE-3862:

    recreate table icesling (
        id integer,
        yearofbirth integer,
        name varchar(30),
        constraint someuniqueconstrain primary key (id)
    );
    commit;

    insert into icesling (id, yearofbirth, name) values ('1', '1980', 'zeca');
    insert into icesling (id, yearofbirth, name) values ('2', '1980', 'chico');
    insert into icesling (id, yearofbirth, name) values ('3', '1983', 'leila');
    commit;

    set list on;

    -- the min() grouping is ignored and 2 records are deleted, only one shoud be deleted
    set count on;

    delete
    from icesling where id in (
        select min(i.id) from icesling i
        where i.yearofbirth=1980
    );
    set count off;

    select 'core-3862' as test_case, t.* from icesling t;
    rollback;

"""

act = isql_act('db', test_script)

expected_stdout = """
    TEST_CASE                       core-92
    X                               0

    TEST_CASE                       core-92
    X                               1


    Records affected: 2

    TEST_CASE                       core-634, case-1
    X                               6

    TEST_CASE                       core-634, case-1
    X                               7

    TEST_CASE                       core-634, case-1
    X                               8

    TEST_CASE                       core-634, case-1
    X                               9

    TEST_CASE                       core-634, case-1
    X                               10


    Records affected: 5

    TEST_CASE                       core-634, case-2
    ID                              1
    VAL                             red

    TEST_CASE                       core-634, case-2
    ID                              3
    VAL                             blue


    Records affected: 2

    TEST_CASE                       core-2799, case-1
    X                               2

    TEST_CASE                       core-2799, case-1
    X                               3

    TEST_CASE                       core-2799, case-1
    X                               4

    TEST_CASE                       core-2799, case-1
    X                               5


    Records affected: 4

    TEST_CASE                       core-2799, case-2
    X                               1

    TEST_CASE                       core-2799, case-2
    X                               2

    TEST_CASE                       core-2799, case-2
    X                               3

    TEST_CASE                       core-2799, case-2
    X                               4


    Records affected: 4
    Records affected: 0

    TEST_CASE                       core-3317
    ID                              1
    ID_PARENT                       2
    ID_CHILD                        2
    DEPTH                           0

    TEST_CASE                       core-3317
    ID                              2
    ID_PARENT                       7
    ID_CHILD                        7
    DEPTH                           0

    TEST_CASE                       core-3317
    ID                              3
    ID_PARENT                       10
    ID_CHILD                        10
    DEPTH                           0

    TEST_CASE                       core-3317
    ID                              6
    ID_PARENT                       7
    ID_CHILD                        10
    DEPTH                           1


    Records affected: 4

    TEST_CASE                       core-3362, case-1
    ID                              1
    F01                             600

    TEST_CASE                       core-3362, case-1
    ID                              2
    F01                             600

    TEST_CASE                       core-3362, case-1
    ID                              3
    F01                             600


    Records affected: 3

    TEST_CASE                       core-3362, case-2
    ID                              1
    F01                             600

    TEST_CASE                       core-3362, case-2
    ID                              2
    F01                             600

    TEST_CASE                       core-3362, case-2
    ID                              3
    F01                             600


    Records affected: 3

    TEST_CASE                       core-3362, case-3
    I                               1
    X                               6
    Y                               5

    TEST_CASE                       core-3362, case-3
    I                               2
    X                               2
    Y                               6

    TEST_CASE                       core-3362, case-3
    I                               3
    X                               3
    Y                               2

    TEST_CASE                       core-3362, case-3
    I                               4
    X                               4
    Y                               3

    TEST_CASE                       core-3362, case-3
    I                               5
    X                               5
    Y                               4


    Records affected: 5

    TEST_CASE                       core-3362, case-4
    X                               <null>
    Y                               5

    TEST_CASE                       core-3362, case-4
    X                               <null>
    Y                               5

    TEST_CASE                       core-3362, case-4
    X                               <null>
    Y                               5

    TEST_CASE                       core-3362, case-4
    X                               <null>
    Y                               5

    TEST_CASE                       core-3362, case-4
    X                               <null>
    Y                               5


    Records affected: 5

    TEST_CASE                       core-3362, case-5
    X                               1
    Y                               15

    TEST_CASE                       core-3362, case-5
    X                               2
    Y                               15

    TEST_CASE                       core-3362, case-5
    X                               3
    Y                               15

    TEST_CASE                       core-3362, case-5
    X                               4
    Y                               15

    TEST_CASE                       core-3362, case-5
    X                               5
    Y                               15


    Records affected: 5

    TEST_CASE                       core-3362, case-6
    X                               1
    Y                               0


    Records affected: 1

    TEST_CASE                       core-3362, case-7
    ID                              1
    X                               <null>

    TEST_CASE                       core-3362, case-7
    ID                              2
    X                               <null>

    TEST_CASE                       core-3362, case-7
    ID                              3
    X                               <null>


    Records affected: 3

    TEST_CASE                       core-3362, case-8.1
    X                               1
    Y                               <null>

    TEST_CASE                       core-3362, case-8.1
    X                               2
    Y                               <null>

    TEST_CASE                       core-3362, case-8.1
    X                               3
    Y                               <null>


    Records affected: 3

    TEST_CASE                       core-3362, case-8.2
    ID                              1
    X                               <null>

    TEST_CASE                       core-3362, case-8.2
    ID                              2
    X                               <null>

    TEST_CASE                       core-3362, case-8.2
    ID                              3
    X                               <null>

    TEST_CASE                       core-3362, case-8.2
    ID                              4
    X                               40


    Records affected: 4

    TEST_CASE                       core-3362, case-9
    X                               1
    Y                               4

    TEST_CASE                       core-3362, case-9
    X                               2
    Y                               4

    TEST_CASE                       core-3362, case-9
    X                               3
    Y                               4

    TEST_CASE                       core-3362, case-9
    X                               4
    Y                               4

    TEST_CASE                       core-3362, case-9
    X                               5
    Y                               4


    Records affected: 5

    TEST_CASE                       core-3362, case-10
    ID                              6
    PID                             <null>

    TEST_CASE                       core-3362, case-10
    ID                              5
    PID                             <null>

    TEST_CASE                       core-3362, case-10
    ID                              4
    PID                             <null>

    TEST_CASE                       core-3362, case-10
    ID                              3
    PID                             <null>

    TEST_CASE                       core-3362, case-10
    ID                              2
    PID                             <null>


    Records affected: 5

    TEST_CASE                       core-3362, case-11a
    ID                              1
    X                               <null>

    TEST_CASE                       core-3362, case-11a
    ID                              2
    X                               <null>

    TEST_CASE                       core-3362, case-11a
    ID                              3
    X                               <null>



    TEST_CASE                       core-3362, case-11b
    ID                              1
    X                               <null>

    TEST_CASE                       core-3362, case-11b
    ID                              2
    X                               <null>

    TEST_CASE                       core-3362, case-11b
    ID                              3
    X                               <null>



    TEST_CASE                       core-3362, case-11c
    ID                              1
    X                               <null>

    TEST_CASE                       core-3362, case-11c
    ID                              2
    X                               <null>

    TEST_CASE                       core-3362, case-11c
    ID                              3
    X                               <null>



    TEST_CASE                       core-3362, case-11d
    ID                              1
    X                               0

    TEST_CASE                       core-3362, case-11d
    ID                              2
    X                               0

    TEST_CASE                       core-3362, case-11d
    ID                              3
    X                               0

    TEST_CASE                       core-3362, case-12a
    ID                              1
    X                               <null>

    TEST_CASE                       core-3362, case-12a
    ID                              2
    X                               <null>

    TEST_CASE                       core-3362, case-12a
    ID                              3
    X                               <null>

    TEST_CASE                       core-3362, case-12a
    ID                              4
    X                               40



    TEST_CASE                       core-3362, case-12b
    ID                              1
    X                               <null>

    TEST_CASE                       core-3362, case-12b
    ID                              2
    X                               <null>

    TEST_CASE                       core-3362, case-12b
    ID                              3
    X                               <null>

    TEST_CASE                       core-3362, case-12b
    ID                              4
    X                               40

    Records affected: 1
    TEST_CASE                       core-3862
    ID                              2
    YEAROFBIRTH                     1980
    NAME                            chico

    TEST_CASE                       core-3862
    ID                              3
    YEAROFBIRTH                     1983
    NAME                            leila

"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

