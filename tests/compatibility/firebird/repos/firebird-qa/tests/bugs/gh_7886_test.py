#coding:utf-8

"""
ID:          issue-7886
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7886
TITLE:       ISQL sets return code to 1 for script that is called via IN command even if this script completes OK. Problem caused by presence of tail comments in the callee
DESCRIPTION:
NOTES:
    Confirmed bug on 6.0.0.154
    Checked on 6.0.0.157 (intermediate snapshot), 5.0.0.1280, 4.0.1.2519
"""

from pathlib import Path
import locale
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

tmp_worker = temp_file('tmp.core_4578.worker.sql')
tmp_caller = temp_file('tmp.core_4578.caller.sql')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_worker: Path, tmp_caller: Path):

    worker_sql = """
        set term ^;
        execute block as
        begin
            -- nop --
        end ^
        set term ;^
        -- this is final comment
    """

    caller_sql = f"""
        set list on;
        commit;
        in "{str(tmp_worker)}";
    """
    tmp_worker.write_text(worker_sql)
    tmp_caller.write_text(caller_sql)
    
    expected_stdout = """
    """

    act.isql(switches=['-q'], input_file = str(tmp_caller), io_enc = locale.getpreferredencoding(), combine_output = True)
    assert act.return_code == 0, f'### BUG ### act.return_code = {act.return_code}'

