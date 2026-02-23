#coding:utf-8

"""
ID:          issue-2241
ISSUE:       2241
TITLE:       Incorrect parser's reaction to the unquoted usage of the keyword "VALUE"
DESCRIPTION:
JIRA:        CORE-1811
FBTEST:      bugs.core_1811
"""

import pytest
from firebird.qa import *

init_script = """recreate table T ( "VALUE" int ) ;
commit;
"""

db = db_factory(init=init_script)

test_script = """delete from T where "VALUE" = 1;
-- OK

delete from T where value = 1 ;
-- ERROR: Illegal use of keyword VALUE
-- This is correct.

delete from T where value = ? ;
-- ERROR: Data type unknown (release build) or assertion failure (debug build)
-- There should be the same error as previously (Illegal use of keyword VALUE)
"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 42000
Dynamic SQL Error
-SQL error code = -901
-Illegal use of keyword VALUE
Statement failed, SQLSTATE = 42000
Dynamic SQL Error
-SQL error code = -901
-Illegal use of keyword VALUE
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

