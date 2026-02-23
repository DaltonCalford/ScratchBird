#coding:utf-8

"""
ID:          issue-3040
ISSUE:       3040
TITLE:       Invalid BLOB ID when working with monitoring tables
DESCRIPTION:
JIRA:        CORE-2632
FBTEST:      bugs.core_2632
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;
    select 1 as k from mon$database;
    set count on;

    select s.mon$sql_text as sql_text_blob
    from mon$statements s
    where s.mon$sql_text NOT containing 'rdb$auth_mapping' -- added 30.03.2017 (4.0.0.x)
    ;
"""

act = isql_act('db', test_script, substitutions=[('SQL_TEXT_BLOB.*', 'SQL_TEXT_BLOB')])

expected_stdout = """
    K                               1
    SQL_TEXT_BLOB
    select 1 as k from mon$database

    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

