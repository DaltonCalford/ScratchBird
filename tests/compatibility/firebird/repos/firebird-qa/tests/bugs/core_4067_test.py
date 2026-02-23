#coding:utf-8

"""
ID:          issue-4395
ISSUE:       4395
TITLE:       Problem with "CREATE DATABASE ... COLLATION ..." and 1 dialect
DESCRIPTION:
JIRA:        CORE-4067
FBTEST:      bugs.core_4067
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [('[ \t]+', ' ')])

expected_stdout = """
    SQL_DIALECT 1
"""

temp_db = temp_file('tmp_4067_1.fdb')

@pytest.mark.version('>=3')
def test_1(act: Action, temp_db: Path):
    test_script = f"""
        set bail on;
        set list on;
        set sql dialect 1;
        create database 'localhost:{str(temp_db)}' page_size 4096 default character set win1251 collation win1251;
        select mon$sql_dialect as sql_dialect from mon$database;
        commit;
        drop database;
    """
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input=test_script, connect_db=False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
