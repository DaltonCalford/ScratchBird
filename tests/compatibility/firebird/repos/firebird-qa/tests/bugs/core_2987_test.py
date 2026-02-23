#coding:utf-8

"""
ID:          issue-3281
ISSUE:       3281
TITLE:       Don't send full length of field over the wire when field is null
DESCRIPTION:
JIRA:        CORE-2897
FBTEST:      bugs.core_2987
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Measurement showed than on 3.0 (SS/SC/CS) transfer of NULLs is more than 5 (five) times
    -- faster than text data with length = 32K. As of 2.5 (SC) than ration is about 1.7 ... 1.8.
    -- Test fills up two tables: one with text data and another with only nulls.
    -- Then we receive data from these tables via ES/EDS, evaluate elapsed time for both cases
    -- and calculate its ratio. This ratio in 3.0 should be not less than 4x.
    create or alter procedure sp_test returns(sel_data_ms int, sel_null_ms int) as begin end;
    create or alter procedure sp_test as begin end;

    recreate global temporary table test1(f01 char(32760)) on commit delete rows;
    recreate global temporary table test2(f01 char(32760)) on commit delete rows;
    commit;


    set term ^;
    create or alter procedure sp_fill as
      declare c_added_rows int = 2000;
    begin
        execute statement
           'insert into test1 '
           || 'with recursive r as (select 0 i from rdb$database union all select r.i+1 from r where r.i<99) '
           || 'select rpad('''', 32760, uuid_to_char(gen_uuid()) ) from r r1,r r2 rows ' || c_added_rows
           on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
           as user 'SYSDBA' password 'masterkey'
           role 'R001' ------------------------------- this will create new attach #1 that will be used later!
        ;
        execute statement
           'insert into test2 '
           || 'with recursive r as (select 0 i from rdb$database union all select r.i+1 from r where r.i<99) '
           || 'select null from r r1,r r2 rows ' || c_added_rows
           on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
           as user 'SYSDBA' password 'masterkey'
           role 'R002' ------------------------------- this will create new attach #2 that will be used later!
        ;
    end
    ^

    create or alter procedure sp_test returns( sel_data_ms int, sel_null_ms int, ratio numeric(10,2) ) as
      declare v_f01 type of column test1.f01;
      declare t0 timestamp;
      declare t1 timestamp;
      declare t2 timestamp;
    begin

        t0='now';
        for
            execute statement
               'select f01 from test1' on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
               as user 'SYSDBA' password 'masterkey'
               role 'R001' --------------- here we use EXISTING attach #1 (from internal FB connection pool)
               into v_f01
        do begin end

        t1='now';

        for
            execute statement
               'select f01 from test2' on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
               as user 'SYSDBA' password 'masterkey'
               role 'R002'  --------------- here we use EXISTING attach #2 (from internal FB connection pool)
               into v_f01
        do begin end

        t2='now';

        sel_data_ms = datediff(millisecond from t0 to t1);
        sel_null_ms = datediff(millisecond from t1 to t2);
        ratio = coalesce( sel_data_ms * 1.000 / nullif(sel_null_ms,0), 9999999.99);

        suspend;

    end
    ^
    set term ;^
    --commit;

    set list on;

    --set stat on;
    -- must be ~15 sec
    execute procedure sp_fill; -- will add data into GTT test1 (in attach #1) and GTT test2 (in attach #2)
    --set stat off;

    set term ^;
    execute block returns( measure_result varchar(40) ) as
      declare min_profit_ratio numeric(3,1) = 3.8;  -- #####  T H R E S H O L D  ####
      declare sel_data_ms int;
      declare sel_null_ms int;
      declare ratio numeric(10,2);
    begin

      -- These four calls will extract rows from GTT test1  (in attach #1) and from GTT test2 (in attach #2).
      -- We have to register statistics only for 2nd...4th measurements due to probable file cache affects.

      select ratio, '' from sp_test into ratio, measure_result;
      --- do not suspend now --
      select sel_data_ms, sel_null_ms, ratio, iif( ratio >= :min_profit_ratio, 'WINS >= '||:min_profit_ratio||'x', 'LOOSES, '||ratio||'x')
        from sp_test
        into sel_data_ms, sel_null_ms,ratio, measure_result;
      suspend;
      select sel_data_ms, sel_null_ms, ratio, iif( ratio >= :min_profit_ratio, 'WINS >= '||:min_profit_ratio||'x', 'LOOSES, '||ratio||'x')
        from sp_test
        into sel_data_ms, sel_null_ms,ratio, measure_result;
      suspend;
      select sel_data_ms, sel_null_ms, ratio, iif( ratio >= :min_profit_ratio, 'WINS >= '||:min_profit_ratio||'x', 'LOOSES, '||ratio||'x')
        from sp_test
        into sel_data_ms, sel_null_ms,ratio, measure_result;
      suspend;
    end
    ^
    set term ;^

    commit;
    --                                    ||||||||||||||||||||||||||||
    -- ###################################|||  FB 4.0+, SS and SC  |||##############################
    --                                    ||||||||||||||||||||||||||||
    -- If we check SS or SC and ExtConnPoolLifeTime > 0 (config parameter FB 4.0+) then current
    -- DB (bugs.core_NNNN.fdb) will be 'captured' by firebird.exe process and fbt_run utility
    -- will not able to drop this database at the final point of test.
    -- Moreover, DB file will be hold until all activity in firebird.exe completed and AFTER this
    -- we have to wait for <ExtConnPoolLifeTime> seconds after it (discussion and small test see
    -- in the letter to hvlad and dimitr 13.10.2019 11:10).
    -- This means that one need to kill all connections to prevent from exception on cleanup phase:
    -- SQLCODE: -901 / lock time-out on wait transaction / object <this_test_DB> is in use
    -- #############################################################################################
    delete from mon$attachments where mon$attachment_id != current_connection;
    commit;

"""

act = isql_act('db', test_script)

expected_stdout = """
   MEASURE_RESULT                  WINS >= 3.8x
   MEASURE_RESULT                  WINS >= 3.8x
   MEASURE_RESULT                  WINS >= 3.8x
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

