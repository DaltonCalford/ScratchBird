#coding:utf-8

"""
ID:          trigger.database.trigger_on_disconnect_04
TITLE:       ON DISCONNECT trigger must fire if attachment is closed by IDLE timeout.
DESCRIPTION: 
    Improvement was introduced 06-apr-2023, commit:
    https://github.com/FirebirdSQL/firebird/commit/d64868cdcd91450e3c58aa9bfa9c1090912ac358
    Test creates table 'detach_audit' which is fulfilled from ON DISCONNECT trigger if current user not SYSDBA.
    We create non-privileged user ('tmp_worker') and make connection to database.
    Also, trigger ON CONNECT is created which establishes session idle timeout to <SESSION_IDLE_TIMEOUT> value.
    When user makes connection, it will be alive only for <SESSION_IDLE_TIMEOUT> seconds, and will be closed
    after this time expired. This *must* cause firing trigger with adding record to 'detach_audit' table.

    Finally, we check that table 'detach_audit' has ONE record (containing name of non-priv user).
NOTES:
    [28.02.2023] pzotov
    1. Confirmed missed trigger firing on 5.0.0.459 (05-apr-2023): table 'detach_audit' remained empty.
    2. It seems that con_worker.is_closed() does not return True when connection is closed because of session
       timeout expiration. This cause to make additional connection ('con_admin') with check content of
       mon$attachments for verifying that attachment 'con_worker' was really closed (cursor must return 0 rows).
    Checked on 5.0.0.961 - all OK.
"""

import pytest
from firebird.qa import *
import time

tmp_worker = user_factory('db', name='tmp_worker', password='123')
db = db_factory()
act = python_act('db')

expected_stdout_chk = """
    ID                              1
    WHO                             TMP_WORKER
    Records affected: 1
"""

SESSION_IDLE_TIMEOUT = 3
WORKER_IN_IDLE_STATE = SESSION_IDLE_TIMEOUT + 2

@pytest.mark.version('>=5.0')
def test_1(act: Action, tmp_worker: User):

    init_sql  = f"""
        recreate table detach_audit(
            id int generated always as identity
            ,dts timestamp default 'now'
            ,who varchar(31) default current_user
        );
        commit;
        grant select, insert on detach_audit to public;

        set term ^;
        create trigger trg_connect on connect as
        begin
           if ( current_user != '{act.db.user}' ) then
               set session idle timeout {SESSION_IDLE_TIMEOUT} second;
        end
        ^
        create trigger trg_disconnect on disconnect as
        begin
           if ( current_user != '{act.db.user}' ) then
               execute statement 'insert into detach_audit default values';
        end
        ^
        set term ;^
        commit;
    """
    act.expected_stdout = ''
    act.isql(switches=['-q'], input = init_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    worker_is_closed = False
    with act.db.connect(no_db_triggers = True) as con_admin, act.db.connect(user = tmp_worker.name, password = tmp_worker.password) as con_worker:
        con_worker_attachment_id = con_worker.info.id
        time.sleep(WORKER_IN_IDLE_STATE)

        if 1==0:
            # NB! PROBLEM EXISTS HERE!
            # con_worker.is_closed() returns FALSE, despite fact that connection was closed because of timeout expiration.
            worker_is_closed = con_worker.is_closed()
        else:
            cur_admin = con_admin.cursor()
            cur_admin.execute(f'select count(*) from mon$attachments where mon$attachment_id = {con_worker_attachment_id}')
            worker_is_closed = True if cur_admin.fetchone()[0] == 0 else False

    assert worker_is_closed, f'### FAIL ### Worker attachment was not closed for {WORKER_IN_IDLE_STATE} second.'

    act.expected_stdout = expected_stdout_chk
    act.isql(switches=['-q'], input = 'set count on;set list on;select id,who from detach_audit;', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
