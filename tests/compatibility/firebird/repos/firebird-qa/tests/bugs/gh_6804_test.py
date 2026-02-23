#coding:utf-8

"""
ID:          issue-6804
ISSUE:       6804
TITLE:       assertion in tomcrypt when key length for rc4 too small
DESCRIPTION:
FBTEST:      bugs.gh_6804
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set list on;
    select encrypt('abc' using rc4 key 'qq') from rdb$database;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 22023
    Invalid key length 2, need >4
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
