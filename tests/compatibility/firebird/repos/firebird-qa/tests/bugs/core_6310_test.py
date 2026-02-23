#coding:utf-8

"""
ID:          issue-6551
ISSUE:       6551
TITLE:       Varchar length limit is not enforced when assigning string with trailing spaces in MBCS
DESCRIPTION:
JIRA:        CORE-6310
FBTEST:      bugs.core_6310
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select char_length(cast(_utf8 '123         ' as varchar(5) character set utf8)) as char_len from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    CHAR_LEN                        5
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
