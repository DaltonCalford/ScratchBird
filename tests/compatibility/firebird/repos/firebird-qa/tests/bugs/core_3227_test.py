#coding:utf-8

"""
ID:          issue-3601
ISSUE:       3601
TITLE:       ASCII_VAL() fails if argument contains multi-byte character anywhere
DESCRIPTION:
JIRA:        CORE-3227
FBTEST:      bugs.core_3227
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """select ascii_val (cast('Hoplala' as char(12) character set utf8)) from rdb$database;
select ascii_val (cast('Hopläla' as char(12) character set utf8)) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
ASCII_VAL
=========
       72


ASCII_VAL
=========
       72

"""

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

