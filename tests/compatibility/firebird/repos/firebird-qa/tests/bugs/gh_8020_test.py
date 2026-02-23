#coding:utf-8

"""
ID:          issue-8020
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8020
TITLE:       AV when both function and dependent table are dropped in the same transaction
DESCRIPTION:
NOTES:
    [12.03.2024] pzotov
        1. Crash occured only when connection is done via TCP protocol.
        2. Another bug currently *remains* in FB 6.x if DROP-statements are in DSQL form, i.e are not 'enclosed' in PSQL and begin/end blocks:
           See https://github.com/FirebirdSQL/firebird/issues/8021 (currently not fixed).
           Because of this, it was decided to run DROP statements within PSQL code.
        3. Test checks whether MON$SERVER_PID remains the same after execution of DROP statements. In case of crash this is not so.
        Confirmed bug on 6.0.0.268. Checked on 6.0.0.269
    [06.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.914.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    test_script = f"""
        set list on;

        set term ^;
        create function f(x int)
          returns int
        as
        begin
          return x;
        end
        ^
        create table t_fn (x int, fx computed by (f(x)))
        ^
        commit
        ^
        set term ;^
        commit;

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        set autoddl off;
        set bail on;
        commit;
        set term ^;
        execute block returns(is_server_pid_the_same boolean) as
            declare v_server_pid_init int;
            declare v_server_pid_curr int;
        begin
            select mon$server_pid from mon$attachments where mon$attachment_id = current_connection into v_server_pid_init;
            begin
                execute statement 'drop function f';
                execute statement 'drop table t_fn';
            when any do
                begin
                end
            end
            select mon$server_pid from mon$attachments where mon$attachment_id = current_connection into v_server_pid_curr;
            is_server_pid_the_same = (v_server_pid_init = v_server_pid_curr);
            suspend;
        end
        ^
        set term ;^
        commit;
    """

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    TEST_FUNC_NAME = 'F' if act.is_version('<6') else  f'{SQL_SCHEMA_PREFIX}"F"'
    expected_stdout = f"""
        IS_SERVER_PID_THE_SAME          <true>
        Statement failed, SQLSTATE = 38000
        unsuccessful metadata update
        -cannot delete
        -Function {TEST_FUNC_NAME}
        -there are 1 dependencies
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
