#coding:utf-8

"""
ID:          issue-2729
ISSUE:       2729
TITLE:       Make mon$statement_id value constant among monitoring snapshots
DESCRIPTION:
JIRA:        CORE-2305
FBTEST:      bugs.core_2305
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(monitoring_att_id int, running_stt_id int);
    commit;

    set term ^;
    execute block as
        declare monitoring_att_id int;
        declare running_stt_id int;
        declare v_dbname varchar(255);
        declare v_stt varchar(1024);
        declare v_usr rdb$user = 'sysdba';
        declare v_pwd varchar(20) = 'masterkey';
        declare v_trn int;
    begin

        v_dbname = 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME');
        v_stt =
            'select current_connection, s.mon$statement_id '
            || 'from mon$statements s '
            || 'where s.mon$transaction_id = :x '
            || 'order by s.mon$statement_id rows 1';
        v_trn = current_transaction;

        execute statement (v_stt) ( x := v_trn )
        into monitoring_att_id, running_stt_id;

        insert into test( monitoring_att_id,  running_stt_id)
                  values(:monitoring_att_id, :running_stt_id);

        execute statement (v_stt) ( x := v_trn )
        on external (v_dbname)
        as user :v_usr password :v_pwd role 'R_001'
        into monitoring_att_id, running_stt_id;

        insert into test( monitoring_att_id,  running_stt_id)
                  values(:monitoring_att_id, :running_stt_id);


        execute statement (v_stt) ( x := v_trn )
        on external (v_dbname)
        as user :v_usr password :v_pwd role 'R_002'
        into monitoring_att_id, running_stt_id;

        insert into test( monitoring_att_id,  running_stt_id)
                  values(:monitoring_att_id, :running_stt_id);

    end
    ^
    set term ;^

    -- select * from test;
    -- Output of select * from test in 2.5.0:
    --    MONITORING_ATT_ID RUNNING_STT_ID
    --    ================= ==============
    --                    6              5
    --                    7             11
    --                    8             11

    -- Output of select * from test since 2.5.1:
    --    MONITORING_ATT_ID RUNNING_STT_ID
    --    ================= ==============
    --                    9            122
    --                   10            122
    --                   11            122

    set list on;
    select
        count(distinct monitoring_att_id) as num_of_mon$statement_observers
       ,count(distinct running_stt_id)    as distinct_statement_id_they_saw
    from test;

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
    NUM_OF_MON$STATEMENT_OBSERVERS  3
    DISTINCT_STATEMENT_ID_THEY_SAW  1
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

