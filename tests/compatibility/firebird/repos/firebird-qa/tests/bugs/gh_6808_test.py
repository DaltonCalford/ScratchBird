#coding:utf-8

"""
ID:          issue-6808
ISSUE:       6808
TITLE:       Segfault in encrypt/decrypt functions when their first argument is NULL
DESCRIPTION:
FBTEST:      bugs.gh_6808
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set heading off;
    select encrypt(null using aes mode cfb key 'AbcdAbcdAbcdAbcd' iv '0123456789012345') from rdb$database;
    select decrypt(null using aes mode ofb key '0123456701234567' iv '1234567890123456') from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    <null>
    <null>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
