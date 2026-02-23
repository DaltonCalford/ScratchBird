#coding:utf-8

"""
ID:          issue-4803
ISSUE:       4803
TITLE:       Changed data not visible in WHEN-section if exception occured inside SP that has been called from this code
DESCRIPTION:
JIRA:        CORE-4483
FBTEST:      bugs.core_4483
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- ISSUE-1: if procedure_A calls procedure_B and the latter makes some DML changes but then fails
    -- then proc_A does NOT see any changes that were made by proc_B.
    -- See letter to dimitr, 02-feb-2015 23:47, attachment: 'core-4483_test1.sql'.

    set term ^;
    create or alter procedure P1 as begin end^
    create or alter procedure P2 as begin end^
    create or alter procedure P3 as begin end^
    create or alter procedure P4 as begin end^
    create or alter procedure P5 as begin end^
    create or alter procedure P6 as begin end^
    commit^

    recreate table t(id int primary key)^
    commit^

    -- P1...P4 -- procedures WITHOUT when blocks and correct code:
    create or alter procedure P1 as begin insert into t values(1); end^
    create or alter procedure P2 as begin insert into t values(2); end^
    create or alter procedure P3 as begin insert into t values(3); end^
    create or alter procedure P4 as begin insert into t values(4); end^


    -- P5 -- single proc WITH when block (but with correct code);
    create or alter procedure P5 as
    begin
      insert into t values(5);
      rdb$set_context('USER_SESSION','PROC_P5_BEFORE_P6_CALL', (select list(id) from t)); --- [ 2 ]
      execute procedure P6;
    when any do
      begin
        rdb$set_context('USER_SESSION','PROC_P5_WHEN_BLK_AFTER_P6_CALL', (select list(id) from t)); --- [ 4 ]
        exception;
      end
    end^

    -- P6 -- deepest procedure with INCORRECT code (where exception occurs):
    create or alter procedure P6 as
      declare i int;
    begin
      insert into t values(6);
      --  ### ERRONEOUS CODE ###
      i = rdb$set_context('USER_SESSION','PROC_P6_ERROR_LINE', (select list(id) from t)) / 0; --- [ 3a ]
    when any do
      begin
        rdb$set_context('USER_SESSION','PROC_P6_WHEN_BLK_CATCHING_ERR', (select list(id) from t)); --- [ 3b ]
        exception;
      end
    end^
    commit^
    set term ;^
    commit;

    set transaction no wait;
    set term ^;
    execute block
    as
      declare i int;
    begin
      delete from t;
      execute procedure P1;
      execute procedure P2;
      execute procedure P3;
      execute procedure P4;
      rdb$set_context('USER_SESSION','MAIN_EB_AFTER_CALL_P1_P4', (select list(id) from t)); --- [ 1 ]

      execute procedure P5; -- ==> update context vars 'PROC_P5_BEFORE_P6_CALL' and 'PROC_P5_WHEN_BLK_AFTER_P6_CALL'

    when any do
      begin
        rdb$set_context('USER_SESSION','MAIN_EB_WHEN_BLK_AFTER_P5_EXC', (select list(id) from t)); --- [ 4 ]
        --exception;
      end
    end
    ^ set term ;^

    SET LIST ON;
    select
        rdb$get_context('USER_SESSION','MAIN_EB_AFTER_CALL_P1_P4')       "main EB after P1,P2,P3,P4"            -- [ 1 ]
       ,rdb$get_context('USER_SESSION','PROC_P5_BEFORE_P6_CALL')         "proc P5 before call P6"            -- [ 2 ]
       ,rdb$get_context('USER_SESSION','PROC_P6_ERROR_LINE')             "proc P6 error source line"                  -- [ 3a ]
       ,rdb$get_context('USER_SESSION','PROC_P6_WHEN_BLK_CATCHING_ERR')  "proc P6 WHEN blk: catch exceptn" -- [ 3b ]
       ,rdb$get_context('USER_SESSION','PROC_P5_WHEN_BLK_AFTER_P6_CALL') "proc P5 WHEN blk: catch exceptn"   -- [ 4 ]
       ,rdb$get_context('USER_SESSION','MAIN_EB_WHEN_BLK_AFTER_P5_EXC')  "main EB WHEN blk: catch exceptn" -- [ 5 ]
    from rdb$database;

    select count(*) t_count from t;
    commit;

    -- #############################################################################################

    -- ISSUE-2: if some procedure performs several DML statements (s1, s2, s3, ..., sN) and N-th DML
    -- fails and control falls into WHEN-block of this SP then we can not see any changes that we
    -- have done inside THIS procedure.
    -- See letter to dimitr, 02-feb-2015 23:47, attachment: 'core-4483_test2.sql'.

    set term ^;
    create or alter procedure p1 as begin end^
    create or alter procedure p2 as begin end^
    create or alter procedure p3 as begin end^
    create or alter procedure p4 as begin end^
    create or alter procedure p5 as begin end^
    create or alter procedure p6 as begin end^
    commit^

    recreate table t(id int)^

    create or alter procedure p1 as
    begin
      insert into t values(1);
      insert into t values(12);
      insert into t values(133);
      rdb$set_context('USER_SESSION','P1_CODE_BEFO_CALL_P2',(select list(id) from t));
      execute procedure p2;
    when any do
      begin
        -- here we must be able to see ALL changes that current procedure has done, i.e.
        -- records with ID = 1, 12 and 133 must be visible for us at this point.
        rdb$set_context('USER_SESSION','P1_CATCH_BLK_SEES',(select list(id) from t));
        --exception;
      end
    end^

    create or alter procedure p2 as
    begin
      insert into t values(2);
      insert into t values(22);
      insert into t values(233);
      rdb$set_context('USER_SESSION','P2_CODE_BEFO_CALL_P3',(select list(id) from t));
      execute procedure p3;
    when any do
      begin
        -- here we must be able to see ALL changes that current procedure has done, i.e.
        -- records with ID = 2, 22 and 233 must be visible for us at this point.
        rdb$set_context('USER_SESSION','P2_CATCH_BLK_SEES',(select list(id) from t));
        exception;
      end
    end^

    create or alter procedure p3 as
    begin
      insert into t values(3);
      insert into t values(32);
      insert into t values(333);
      rdb$set_context('USER_SESSION','P3_CODE_BEFO_CALL_P4',(select list(id) from t));
      execute procedure p4;
    when any do
      begin
        -- here we must be able to see ALL changes that current procedure has done, i.e.
        -- records with ID = 3, 32 and 333 must be visible for us at this point.
        rdb$set_context('USER_SESSION','P3_CATCH_BLK_SEES',(select list(id) from t));
        exception;
      end
    end^

    create or alter procedure p4 as
    begin
      insert into t values(4);
      insert into t values(42);
      insert into t values(433);
      rdb$set_context('USER_SESSION','P4_CODE_BEFO_CALL_P5',(select list(id) from t));
      execute procedure p5;
    when any do
      begin
        -- here we must be able to see ALL changes that current procedure has done, i.e.
        -- records with ID = 4, 42 and 433 must be visible for us at this point.
        rdb$set_context('USER_SESSION','P4_CATCH_BLK_SEES',(select list(id) from t));
        exception;
      end
    end^

    create or alter procedure p5 as
    begin
      insert into t values(5);
      insert into t values(52);
      insert into t values(533);
      rdb$set_context('USER_SESSION','P5_CODE_BEFO_CALL_P6',(select list(id) from t));
      execute procedure p6;
    when any do
      begin
        -- here we must be able to see ALL changes that current procedure has done, i.e.
        -- records with ID = 5, 52 and 533 must be visible for us at this point.
        rdb$set_context('USER_SESSION','P5_CATCH_BLK_SEES',(select list(id) from t));
        exception;
      end
    end^

    create or alter procedure p6 as
      declare i int;
    begin
      insert into t values(6);
      insert into t values(62);
      insert into t values(633);
      rdb$set_context('USER_SESSION','P6_CODE_BEFO_ZERO_DIV',(select list(id) from t));
      i = 1/0;
    when any do
      begin
        -- here we must be able to see ALL changes that current procedure has done, i.e.
        -- records with ID = 6, 62 and 633 must be visible for us at this point.
        rdb$set_context('USER_SESSION','P6_CATCH_BLK_SEES',(select list(id) from t));
        exception;
      end
    end^

    commit^
    set term ;^

    execute procedure p1;

    set list on;
    select
       rdb$get_context('USER_SESSION', 'P6_CODE_BEFO_ZERO_DIV') as "IDs in p6 before zero div"
      ,rdb$get_context('USER_SESSION', 'P6_CATCH_BLK_SEES') as "IDs in p6 catch blk"

      ,rdb$get_context('USER_SESSION', 'P5_CODE_BEFO_CALL_P6') as "IDs in p5 before p6 call"
      ,rdb$get_context('USER_SESSION', 'P5_CATCH_BLK_SEES') as "IDs in p5 catch blk"

      ,rdb$get_context('USER_SESSION', 'P4_CODE_BEFO_CALL_P5') as "IDs in p4 before p5 call"
      ,rdb$get_context('USER_SESSION', 'P4_CATCH_BLK_SEES') as "IDs in p4 catch blk"

      ,rdb$get_context('USER_SESSION', 'P3_CODE_BEFO_CALL_P4') as "IDs in p3 before p4 call"
      ,rdb$get_context('USER_SESSION', 'P3_CATCH_BLK_SEES') as "IDs in p3 catch blk"

      ,rdb$get_context('USER_SESSION', 'P2_CODE_BEFO_CALL_P3') as "IDs in p2 before p3 call"
      ,rdb$get_context('USER_SESSION', 'P2_CATCH_BLK_SEES') as "IDs in p2 catch blk"

      ,rdb$get_context('USER_SESSION', 'P1_CODE_BEFO_CALL_P2') as "IDs in p1 before p2 call"
      ,rdb$get_context('USER_SESSION', 'P1_CATCH_BLK_SEES') as "IDs in p1 catch blk"

    from rdb$database;
    set list off;
"""

act = isql_act('db', test_script)

expected_stdout = """
    main EB after P1,P2,P3,P4       1,2,3,4
    proc P5 before call P6          1,2,3,4,5
    proc P6 error source line       1,2,3,4,5,6
    proc P6 WHEN blk: catch exceptn 1,2,3,4,5,6
    proc P5 WHEN blk: catch exceptn 1,2,3,4,5
    main EB WHEN blk: catch exceptn 1,2,3,4
    T_COUNT                         4

    IDs in p6 before zero div       1,12,133,2,22,233,3,32,333,4,42,433,5,52,533,6,62,633
    IDs in p6 catch blk             1,12,133,2,22,233,3,32,333,4,42,433,5,52,533,6,62,633
    IDs in p5 before p6 call        1,12,133,2,22,233,3,32,333,4,42,433,5,52,533
    IDs in p5 catch blk             1,12,133,2,22,233,3,32,333,4,42,433,5,52,533
    IDs in p4 before p5 call        1,12,133,2,22,233,3,32,333,4,42,433
    IDs in p4 catch blk             1,12,133,2,22,233,3,32,333,4,42,433
    IDs in p3 before p4 call        1,12,133,2,22,233,3,32,333
    IDs in p3 catch blk             1,12,133,2,22,233,3,32,333
    IDs in p2 before p3 call        1,12,133,2,22,233
    IDs in p2 catch blk             1,12,133,2,22,233
    IDs in p1 before p2 call        1,12,133
    IDs in p1 catch blk             1,12,133
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

