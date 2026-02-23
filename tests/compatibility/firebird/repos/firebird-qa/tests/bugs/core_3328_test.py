#coding:utf-8

"""
ID:          issue-3694
ISSUE:       3694
TITLE:       Client writes error messages into firebird.log when database is shutted down
DESCRIPTION:
  Difference between old and new firebird.log should _NOT_ contain lines with words 'gds__detach' or 'lost'.
  If these words absent - all fine, actual and expected output both have to be empty.
JIRA:        CORE-3328
FBTEST:      bugs.core_3328
NOTES:
    [13.09.2022] pzotov
    Checked on Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.691
"""

import pytest
import time
from difflib import unified_diff
import re
from threading import Thread
from firebird.qa import *
from firebird.driver import ShutdownMethod, ShutdownMode

init_script = """
    create table test(s varchar(36) unique);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('database.*shutdown', 'database shutdown')])


def run_work(act: Action):
    expected_stderr = """
        Statement failed, SQLSTATE = HY000
        database test.fdb shutdown
        Statement failed, SQLSTATE = HY000
        database test.fdb shutdown
    """
    test_script = """
    show version;
    set term ^;
    execute block as
        declare v_role varchar(31);
    begin
        v_role = left(replace( uuid_to_char(gen_uuid()), '-', ''), 31);
        while (1=1) do
        begin
            insert into test(s) values( uuid_to_char( gen_uuid() ) );
        end
    end
    ^
    set term ;^
    """
    act.expected_stderr = expected_stderr
    act.isql(switches=['-n'], input=test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.connect_server() as srv:
        log_before = act.get_firebird_log()

        work_thread = Thread(target=run_work, args=[act])
        work_thread.start()
        time.sleep(2)
        srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.FULL,
                              method=ShutdownMethod.FORCED, timeout=0)
        srv.database.bring_online(database=act.db.db_path)

        work_thread.join(2)
        if work_thread.is_alive():
            pytest.fail('Work thread is still alive')

        log_after = act.get_firebird_log()
        log_diff = list(unified_diff(log_before, log_after))

        p_dbshut = re.compile('database.*shutdown', re.IGNORECASE)
        p_diff = [p for p in log_diff if p.startswith('+') and p_dbshut.search(p)]
        if p_diff:
            print('UNEXPECTED LINES IN LOG DIFF:')
            for p in p_diff:
                print(p)

        assert '' == capsys.readouterr().out
