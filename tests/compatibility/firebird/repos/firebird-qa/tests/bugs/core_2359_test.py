#coding:utf-8

"""
ID:          issue-2782
ISSUE:       2782
TITLE:       Logical multibyte maximum string length is not respected when assigning numbers
DESCRIPTION:
JIRA:        CORE-2359
FBTEST:      bugs.core_2359
"""

import pytest
from firebird.qa import *

init_script = """create table t (c varchar(2) character set utf8);
"""

db = db_factory(init=init_script)

test_script = """insert into t values ('aaaaaaaa'); -- error: ok
insert into t values (12345678); -- pass: not ok
"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 22001
arithmetic exception, numeric overflow, or string truncation
-string right truncation
-expected length 2, actual 8
Statement failed, SQLSTATE = 22001
arithmetic exception, numeric overflow, or string truncation
-string right truncation
-expected length 2, actual 8
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

