#coding:utf-8

"""
ID:          issue-1912
ISSUE:       1912
TITLE:       New builtin function DATEADD() implements wrong choice of keywords for expanded syntax
DESCRIPTION:
JIRA:        CORE-1497
FBTEST:      bugs.core_1497
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT DATEADD(1 DAY TO date '29-Feb-2012')
,DATEADD(1 MONTH TO date '29-Feb-2012')
,DATEADD(1 YEAR TO date '29-Feb-2012')
FROM RDB$DATABASE;"""

act = isql_act('db', test_script)

expected_stdout = """
    DATEADD     DATEADD     DATEADD
=========== =========== ===========
2012-03-01  2012-03-29  2013-02-28

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

