#coding:utf-8

"""
ID:          issue-6237
ISSUE:       6237
TITLE:       Regression: ROLE does not passed in ES/EDS (specifying it in the statement is ignored)
DESCRIPTION:
JIRA:        CORE-5985
FBTEST:      bugs.core_5985
"""

import pytest
from firebird.qa import *

db = db_factory()

user_foo = user_factory('db', name='tmp$c5985_foo', password='123')
user_bar = user_factory('db', name='tmp$c5985_bar', password='456')

test_script = """
    create role worker;
    create role manager;
    commit;

    grant worker to tmp$c5985_foo;
    grant manager to tmp$c5985_bar;
    commit;

    connect '$(DSN)' user 'tmp$c5985_bar' password '456' role manager;

    set list on;
    select mon$user who_am_i, mon$role whats_my_role
    from mon$attachments
    where mon$attachment_id = current_connection;

    set term ^;
    execute block returns(who_am_i varchar(31), whats_my_role varchar(31)) as
        declare v_sttm varchar(2048);
        declare v_user varchar(31) = 'tmp$c5985_foo';
        declare v_pswd varchar(31) = '123';
        declare v_role varchar(31) = 'WORKER';
        declare v_extd varchar(255);
    begin
        v_extd = 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME');
        v_sttm = 'select mon$user, mon$role from mon$attachments where mon$attachment_id = current_connection';

        execute statement
            v_sttm
            on external v_extd
            as user v_user password v_pswd role v_role
        into
            who_am_i, whats_my_role;

        suspend;

    end
    ^
    set term ;^
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';
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
    WHO_AM_I                        TMP$C5985_BAR
    WHATS_MY_ROLE                   MANAGER

    WHO_AM_I                        TMP$C5985_FOO
    WHATS_MY_ROLE                   WORKER
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action, user_foo, user_bar):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
