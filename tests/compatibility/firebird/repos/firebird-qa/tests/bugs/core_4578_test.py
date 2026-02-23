#coding:utf-8

"""
ID:          issue-4894
ISSUE:       4894
TITLE:       INPUT file not properly closed
DESCRIPTION:
    Test makes TWO attempts to execute script using 'IN <script>' command.
    After first attempt (which must complete OK) we try to delete file using Windows 'DEL' command.
    This command must finish successful and second attempt to run the same script must file with
    message 'Unable to open ...'

JIRA:        CORE-4578
FBTEST:      bugs.core_4578
"""

from pathlib import Path
import locale
import pytest
from firebird.qa import *

init_sql = """
    recreate table test(id bigint primary key);
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db', substitutions=[('Unable to open.*', 'Unable to open')])


tmp_worker = temp_file('tmp.core_4578.worker.sql')
tmp_caller = temp_file('tmp.core_4578.caller.sql')

@pytest.mark.version('>=3')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_worker: Path, tmp_caller: Path):


    worker_sql = """
        set list on;
        insert into test(id) values(1) returning id;
        commit;
    """

    caller_sql = f"""
        set list on;
        commit;
        in "{str(tmp_worker)}";
        shell del "{str(tmp_worker)}";
        in "{str(tmp_worker)}";
    """
    tmp_worker.write_text(worker_sql)
    tmp_caller.write_text(caller_sql)
    
    expected_stdout = """
        ID                              1
        Unable to open
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input_file = str(tmp_caller), io_enc = locale.getpreferredencoding(), combine_output = True)

    assert act.clean_stdout == act.clean_expected_stdout
