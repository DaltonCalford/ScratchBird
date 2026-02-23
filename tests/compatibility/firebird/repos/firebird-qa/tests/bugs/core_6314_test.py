#coding:utf-8

"""
ID:          issue-6555
ISSUE:       6555
TITLE:       Assigning RDB$DB_KEY to MBCS CHAR/VARCHAR does not enforce the target limit
DESCRIPTION:
  In order to prevent receiving non-ascii characters in output we try to get only octet_length of this.
  Confirmed bug on 3.0.6.33289, 4.0.0.1954 (get result = 2 instead of raising error).
JIRA:        CORE-6314
FBTEST:      bugs.core_6314
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select octet_length(x) as cast_dbkey_to_char2_length from (select cast(rdb$db_key as char(2) character set utf8) x from rdb$database);
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 2, actual 8
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
