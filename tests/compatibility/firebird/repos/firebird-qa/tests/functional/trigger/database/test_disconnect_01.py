#coding:utf-8

"""
ID:          trigger.database.disconnect
TITLE:       Trigger on database disconnect: check that exception that raised when trigger fires is written to firebird.log
DESCRIPTION:
    Test covers https://github.com/FirebirdSQL/firebird/issues/4282
    Discussed with Alex, 16.12.2020 functionality that was not specified in the documentation:
    exception that raises in a trigger on DISCONNECT reflects in the firebird.log.

    Test creates trigger on disconnect and put in its body statement which always will fail: 1/0.
    Then we get content of firebird.log before disconnect and after.
    Finally we compare these logs and search in the difference lines about error message.
FBTEST:      functional.trigger.database.disconnect_01
NOTES:
    [26.05.2022] pzotov
        Re-implemented for work in firebird-qa suite. 
        ACHTUNG: firebird.log may contain NON-ASCII characters if localized Windows is used!
        Because of this, we have to add 'encoding=locale.getpreferredencoding()' to act.connect-server() call.
        Checked on: 4.0.1.2692, 5.0.0.497
"""

import pytest
from firebird.qa import *
import time
from difflib import unified_diff
import re
import locale

tmp_worker = user_factory('db', name='tmp_worker', password='123')

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' '), ('line: \\d+, col: \\d+', '')])

expected_stdout_test_sql = """
    WHO_AM_I TMP_WORKER
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_worker: User, capsys):

    init_sql  = f"""
        set term ^;
        create trigger trg_disconnect on disconnect as
            declare n int;
        begin
           if ( current_user = '{act.db.user}' ) then
               exit;
            n = 1/0;
        end
        ^
        set term ;^
        commit;
    """
    act.isql(switches=['-q'], input = init_sql)

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_init = srv.readlines()

    act.expected_stdout = expected_stdout_test_sql
    check_sql="""
        set list on;
        select current_user who_am_i from rdb$database;
    """
    act.isql(switches=['-user', tmp_worker.name, '-password', tmp_worker.password, act.db.dsn], connect_db=False, credentials=False, input = check_sql, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    time.sleep(1)
    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_curr = srv.readlines()

    diff_patterns = [
        "Error at disconnect:",
        "arithmetic exception, numeric overflow, or string truncation",
        "Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.",
        "At trigger",
    ]
    diff_patterns = [re.compile(s) for s in diff_patterns]

    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+'):
            if act.match_any(line, diff_patterns):
                print(line.strip())

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_TRG_NAME = "'TRG_DISCONNECT'" if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TRG_DISCONNECT"'
    expected_stdout_log_diff = f"""
        + Error at disconnect:
        + arithmetic exception, numeric overflow, or string truncation
        + Integer divide by zero. The code attempted to divide an integer value by an integer divisor of zero.
        + At trigger {TEST_TRG_NAME}
    """

    act.expected_stdout = expected_stdout_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
