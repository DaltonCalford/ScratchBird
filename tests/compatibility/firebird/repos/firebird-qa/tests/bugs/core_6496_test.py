#coding:utf-8

"""
ID:          issue-6726
ISSUE:       6726
TITLE:       string_to_datetime and '\\0' symbol
DESCRIPTION:
  ascii_char(0) was allowed to be concatenated with string and pass then to cast(... as timestamp)
  up to 4.0.0.1227 (29-09-2018), and is forbidden since 4.0.0.1346 (17.12.2018).
  FB 3.x allows this character to be used and issues timestamp instead of error.
JIRA:        CORE-6496
FBTEST:      bugs.core_6496
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
  set heading off;
  select cast('5.3.2021 01:02:03.1234' || ascii_char(0) as timestamp) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 22009
    Invalid time zone region:
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
