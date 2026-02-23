#coding:utf-8

"""
ID:          n/a
ISSUE:       None
TITLE:       Check support of SAVEPOINTS in local temporary tables.
DESCRIPTION:
    This test uses ideas from several other tests related to savepoints.
NOTES:
    [08.02.2025] pzotov
    Checked on 6.0.0.1405
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- tests/functional/sqlite/test_7f7f8026ed.py
    set bail OFF;
    set list on;
    create local temporary table t1(x int not null, y char(10)) on commit preserve rows;
    create unique index t1_x_unq on t1(x);

    insert into t1(x,y) 
    with recursive c(x) as (select 1 x from rdb$database union all select x+1 from c where x<250)
    select x*10, x || '*'  from c;
    savepoint p1;

    select count(*) as test_1_cnt_point_1a from t1;

    insert into t1(x,y)
    with recursive c(x) as (select 1 x from rdb$database union all select x+1 from c where x<250)
    select x*10+1, x || '*'  from c;

    rollback to p1;

    select count(*) as test_1_cnt_point_1b from t1;

    savepoint p2;

    insert into t1(x,y)
    with recursive c(x) as (select 1 x from rdb$database union all select x+1 from c where x<10)
    select x*10+2, x || '*' from c;

    rollback to p2;
    release savepoint p1 only;
    commit;
    select count(*) as test_1_cnt_final from t1;
    commit;

    -- ################################################
    -- tests/bugs/gh_5009_test.py 
    create local temporary table t2 (f integer) on commit preserve rows;
    create index t2_f on t2 (f);
    insert into t2 values (1);
    commit;
    update t2 set f=2;
    savepoint a;
    update t2 set f=3;
    savepoint b;
    update t2 set f=3;
    savepoint c;
    update t2 set f=4;
    savepoint d;
    update t2 set f=4;
    release savepoint b only;
    rollback to savepoint c;
    commit;

    set count on;
    set plan on;
    select g.f as test_2_nr from t2 g;
    select g.f as test_2_ir from t2 g where g.f between 1 and 4;
    set count off;
    set plan off;

    --##################################################
    -- tests/bugs/core_1830_test.py

	create local temporary table t3(test3_id char(1), test3_name varchar(255)) on commit preserve rows;

	create index t3_id on t3 (test3_id);
	create exception exc_wrong 'Something wrong occurs...';
	commit ;

	insert into t3 (test3_id) values ('1');
	commit;

	select * from t3 where test3_id = '1';
	set term ^;
	execute block as
	begin
        update t3 set test3_name = 'xxx';
        update t3 set test3_id = '2';
        exception exc_wrong;
	end ^
	set term ;^
	set count on;
	select 'test_3-point-1' as msg, t3.* from t3 ;
	select 'test_3-point-2' as msg, t3.* from t3 where test3_id = '1' ;
	commit;
	select 'test_3-point-3' as msg, t3.* from t3 ;
	set count off;

	--###################################################
	-- tests/bugs/gh_7651_test.py
    recreate local temporary table t4 (
        test4_id int not null
       ,test4_val int not null
    ) on commit preserve rows;
    create unique index t4_id_unq on t4(test4_id);

    savepoint test4_sv;
    insert into t4 values (1, (select count(*) from t4 where test4_id = 1)) returning test4_id, test4_val;
    release savepoint test4_sv;
    select 'Completed' as test4_msg from rdb$database;

    --###################################################
    -- tests/bugs/core-4424_test.py
    recreate local temporary table t5 (f_value int) on commit preserve rows;
    recreate table err_log(f_value int, code_point varchar(100));
    commit;

    insert into t5 values (1);
    commit;

    set term ^;
    execute block as
        declare a int;
        declare g int;
        declare s varchar(100);
    begin
        execute statement ('update t5 set f_value = ?') (2);
        begin
            execute statement ('update t5 set f_value = ?') (3);
            begin
                execute statement ('update t5 set f_value = ?') (4);
                begin
                    execute statement ('update t5 set f_value = ?') (5);
                    begin
                        execute statement ('update t5 set f_value = ?') (6);
                        begin
                            execute statement ('update t5 set f_value = f_value / ?') (0);
                        -- NO 'when' here! Exception should pass to upper blobk with 'set f_value=6'
                        end
                    -- NO 'when' here! Exception should pass to upper blobk with 'set f_value=5'
                    end
                -- NO 'when' here! Exception should pass to upper blobk with 'set f_value=4'
                end
            -- At this point:
            -- 1) table 't5' must contain f_value = 4
            -- 2) we just get exception from most nested level.
            -- Now we must log which 'when' sections was in use for handling exception.
            -- Also, we want to check that exception *INSIDE* last 'when' section will be also handled.
            when gdscode arith_except do
                begin
                    s = 'Fall in point_A: "WHEN GDS ARITH"';
                    rdb$set_context('USER_SESSION', s, 'gds='||gdscode );
                    insert into err_log (f_value, code_point)
                    select f_value, :s || ', gdscode='||gdscode
                    from t5;
                end
            when sqlcode -802 do
                begin
                    s = 'Fall in point_B: "WHEN SQLCODE ' ||sqlcode  || '"';
                    rdb$set_context('USER_SESSION', s, 'gds='||gdscode );
                    insert into err_log (f_value, code_point)
                    select f_value, :s || ', gdscode='||gdscode
                    from t5;
                end
            when any do
                begin
                    s = 'Fall in point_C: "WHEN ANY", 1st (inner)';
                    rdb$set_context('USER_SESSION', s, 'gds='||gdscode );
                    insert into err_log (f_value, code_point)
                    select f_value, :s || ', gdscode='||gdscode
                    from t5;
                end
            when any do
                begin

                    rdb$set_context('USER_SESSION','Fall in point_D: "WHEN ANY", 2nd (inner)', 'gds='||gdscode );

                    a = 1/0; -- ###  :::  !!!  NB !!!  :::   ###  EXCEPTION WILL BE HERE!

                    -- NB:
                    -- Previous statement should raise anothernew exception
                    -- which will force engine to UNDO t5.f_value from 4 to 3.
                    -- FB 3.0 will NOT do this (checked on WI-V3.0.1.32575), FB 4.0 works fine.
                end
            end
        when any do
            begin
                s = 'Fall in point_E: "WHEN ANY", final (outer)';
                rdb$set_context('USER_SESSION', s, 'gds='||gdscode  );
                insert into err_log (f_value, code_point)
                select f_value, :s || ', gds='||gdscode
                from t5;
            end
        end
    end
    ^
    set term ;^
    commit;

    select f_value as test_5_value from t5;

    select f_value, code_point from err_log;

    select mon$variable_name as ctx_name, mon$variable_value as ctx_val
    from mon$context_variables c
    where c.mon$attachment_id = current_connection;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    TEST_1_CNT_POINT_1A 250
    TEST_1_CNT_POINT_1B 250
    TEST_1_CNT_FINAL 250

    PLAN ("G" NATURAL)
    TEST_2_NR 3
    Records affected: 1
    PLAN ("G" INDEX ("PUBLIC"."T2_F"))
    TEST_2_IR 3
    Records affected: 1

    TEST3_ID 1
    TEST3_NAME <null>
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EXC_WRONG"
    -Something wrong occurs...
    -At block line: 5, col: 9
    MSG test_3-point-1
    TEST3_ID 1
    TEST3_NAME <null>
    Records affected: 1
    MSG test_3-point-2
    TEST3_ID 1
    TEST3_NAME <null>
    Records affected: 1
    MSG test_3-point-3
    TEST3_ID 1
    TEST3_NAME <null>
    Records affected: 1

    TEST4_ID 1
    TEST4_VAL 0
    TEST4_MSG Completed

    TEST_5_VALUE 3
    F_VALUE 3
    CODE_POINT Fall in point_E: "WHEN ANY", final (outer), gds=335544321
    CTX_NAME Fall in point_A: "WHEN GDS ARITH"
    CTX_VAL gds=335544321
    CTX_NAME Fall in point_C: "WHEN ANY", 1st (inner)
    CTX_VAL gds=0
    CTX_NAME Fall in point_D: "WHEN ANY", 2nd (inner)
    CTX_VAL gds=0
    CTX_NAME Fall in point_E: "WHEN ANY", final (outer)
    CTX_VAL gds=335544321
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
