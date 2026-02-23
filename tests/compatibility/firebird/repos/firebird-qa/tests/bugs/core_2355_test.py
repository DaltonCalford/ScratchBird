#coding:utf-8

"""
ID:          issue-2778
ISSUE:       2778
TITLE:       Incorrect handling of LOWER/UPPER when result string shrinks in terms of byte length
DESCRIPTION:
JIRA:        CORE-2355
FBTEST:      bugs.core_2355
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """SELECT LOWER('İA') FROM RDB$DATABASE;
SELECT LOWER('AӴЁΪΣƓİ') FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
LOWER
======
ia


LOWER
=======
aӵёϊσɠi

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

