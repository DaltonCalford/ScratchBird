#coding:utf-8

"""
ID:          issue-1789
ISSUE:       1789
TITLE:       Execute block fails within execute statement
DESCRIPTION:
JIRA:        CORE-1371
FBTEST:      bugs.core_1371
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """set term ^;
create procedure P
as
begin
  execute statement 'execute block as begin end';
end ^

set term ;^
commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
