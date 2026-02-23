#coding:utf-8

"""
ID:          issue-2438
ISSUE:       2438
TITLE:       When trying to show "conversion error", "arithmetic exception/string truncation" may appear instead, misleading the user
DESCRIPTION:
JIRA:        CORE-2001
FBTEST:      bugs.core_2001
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select cast('1995' as date) from rdb$database;
select cast('1995-12-2444444444444444444444444444444' as date) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
       CAST
===========

       CAST
==========="""

expected_stderr = """Statement failed, SQLSTATE = 22018

conversion error from string "1995"

Statement failed, SQLSTATE = 22018

conversion error from string "1995-12-2444444444444444444444444444444"

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

