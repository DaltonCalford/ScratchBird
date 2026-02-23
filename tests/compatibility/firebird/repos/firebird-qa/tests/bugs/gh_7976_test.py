#coding:utf-8

"""
ID:          issue-7976
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7976
TITLE:       False validation error for short unpacked records
DESCRIPTION:
NOTES:
    [25.01.2024] pzotov
    Confirmed bug on 5.0.1.1318, 6.0.0.223.
    Checked on 5.0.1.1324.
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_sql = """
    create table tmp1(a1 integer);
    insert into tmp1 values (1000);
    commit;
"""
db = db_factory(init = init_sql)

act = python_act('db')

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, capsys):

    act.expected_stdout = ""
    act.gfix(switches=['-v', '-full', str(act.db.dsn)])
    assert act.clean_stdout == act.clean_expected_stdout
