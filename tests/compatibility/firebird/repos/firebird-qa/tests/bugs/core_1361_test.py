#coding:utf-8

"""
ID:          issue-1779
ISSUE:       1779
TITLE:       Index operations for global temporary tables are not visible for the active connection
DESCRIPTION:
JIRA:        CORE-1361
FBTEST:      bugs.core_1361
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure sp_check_plan returns(sql_plan blob) as
    begin
    end
    ^
    execute block as
    begin
      begin execute statement 'drop role poor_dba'; when any do begin end end
      begin execute statement 'drop role cool_dba'; when any do begin end end
      begin execute statement 'drop role super_dba'; when any do begin end end
    end
    ^
    set term ;^
    commit;

    recreate global temporary table gtt_session(x int, y int) on commit preserve rows;
    commit;
    create role poor_dba;
    create role cool_dba;
    create role super_dba;
    commit;

    grant poor_dba to sysdba;  -- for connect #1
    grant cool_dba to sysdba;  -- for connect #2
    grant super_dba to sysdba; -- for connect #3 (index also WILL be seen for it)
    commit;

    set term ^;
    create or alter procedure sp_check_plan returns(sql_plan blob) as
        declare n int;
    begin
        insert into gtt_session
        select rand()*100, rand()*100
        from rdb$types,rdb$triggers
        union all
        select -2, -3
        from rdb$database;

        execute statement
            'select '
            || '    (select count(*) from gtt_session g where g.x + g.y = -5) '
            || '   ,mon$explained_plan '
            || 'from mon$statements s '
            || 'where s.mon$transaction_id = current_transaction and mon$explained_plan containing ''GTT_SESSION'' '
            || 'rows 1'
            into n, sql_plan;
        suspend;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set blob all;
    set term ^;
    execute block returns(using_index smallint) as
       declare v_dbname type of column mon$database.mon$database_name;
       declare v_usr varchar(31) = 'SYSDBA';
       declare v_pwd varchar(20) = 'masterkey';
       declare role_1 varchar(31) = 'POOR_DBA';
       declare role_2 varchar(31) = 'COOL_DBA';
       declare role_3 varchar(31) = 'SUPER_DBA';
       declare sql_plan blob;
    begin

       v_dbname = 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME');

       execute statement 'select sql_plan from sp_check_plan'
       on external v_dbname
       as user v_usr password v_pwd role upper( role_1 )
       into sql_plan;
       using_index = iif(sql_plan containing 'GTT_SESSION_X_Y', 1, 0);
       suspend;

       --------------------------------------------------------

       execute statement 'create index gtt_session_x_y on gtt_session computed by ( x+y )'
       with autonomous transaction
       on external v_dbname
       as user v_usr password v_pwd role upper( role_2 );
       -- nb: changing this to 'role_1' will produce in firebird.log:
       -- internal Firebird consistency check (invalid SEND request (167),
       -- file: JrdStatement.cpp line: 325)

       execute statement 'select sql_plan from sp_check_plan'
       on external v_dbname
       as user v_usr password v_pwd role upper( role_2 )
       into sql_plan;
       using_index = iif(sql_plan containing 'GTT_SESSION_X_Y', 1, 0);
       suspend;

       --------------------------------------------------------

       execute statement 'select sql_plan from sp_check_plan'
       on external v_dbname
       as user v_usr password v_pwd role upper( role_3 )
       into sql_plan;
       using_index = iif(sql_plan containing 'GTT_SESSION_X_Y', 1, 0);
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
    USING_INDEX                     0
    USING_INDEX                     1
    USING_INDEX                     1
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

