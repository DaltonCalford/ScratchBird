#coding:utf-8

"""
ID:          issue-3025
ISSUE:       3025
TITLE:       Silent truncation when using utf8 parameters and utf8 client character set encoding
DESCRIPTION:
JIRA:        CORE-2615
FBTEST:      bugs.core_2615
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test (c CHAR(10) CHARACTER SET UTF8);
COMMIT;"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """INSERT INTO test VALUES ('012345679012345');
COMMIT;"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 22001
arithmetic exception, numeric overflow, or string truncation
-string right truncation
-expected length 10, actual 15
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

