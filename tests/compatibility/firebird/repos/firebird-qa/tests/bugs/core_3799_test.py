#coding:utf-8

"""
ID:          issue-4142
ISSUE:       4142
TITLE:       with caller privileges option do not work with autonomous transaction option
DESCRIPTION:
JIRA:        CORE-3799
FBTEST:      bugs.core_3799
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow to specify THE SAME terminator twice,
    i.e.
    set term @; select 1 from rdb$database @ set term @; - will not compile ("Unexpected end of command" raises).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    -- Drop old account if it remains from prevoius run:
    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop user tmp$c3799' with autonomous transaction;
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create user tmp$c3799 password '123';
    commit;

    revoke all on all from tmp$c3799;
    commit;

    create or alter procedure sp_test as begin end;
    recreate table test(whoami rdb$user, my_trn int default current_transaction, outer_trn int);
    commit;

    set term ^;
    create or alter procedure sp_test as
    begin
       execute statement ('insert into test(whoami, outer_trn) values(:x, :y)')
               ( x := current_user , y := current_transaction )
               with caller privileges
               with autonomous transaction;
    end
    ^
    set term ;^
    commit;

    grant insert,select on table test to procedure sp_test;
    grant execute on procedure sp_test to user tmp$c3799;
    commit;

    -------------------------------------------------------------------------------------------------

    set term ^;
    execute block as
        declare v_usr rdb$user = 'tmp$c3799';
        declare v_pwd rdb$user = '123';
    begin
        execute statement 'execute procedure sp_test' as user v_usr password v_pwd;
    end
    ^
    set term ;^
    commit;

    set list on;
    select whoami as "Who am I ?", sign( my_trn - outer_trn ) "Did I work in AUTONOMOUS Tx ?"
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

    drop user tmp$c3799;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Who am I ?                      TMP$C3799
    Did I work in AUTONOMOUS Tx ?   1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

