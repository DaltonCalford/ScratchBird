#coding:utf-8

"""
ID:          issue-4510
ISSUE:       4510
TITLE:       Executing empty EXECUTE BLOCK with NotNull output parameter raised error
DESCRIPTION:
JIRA:        CORE-4184
FBTEST:      bugs.core_4184
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    execute block
    returns (id integer not null)
    as
    begin
    end;
    -- Output in 2.5.0 ... 2.5.4:
    --          ID
    --============
    --Statement failed, SQLSTATE = 42000
    --validation error for variable ID, value "*** null ***"
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
