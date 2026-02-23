#coding:utf-8

"""
ID:          issue-2475
ISSUE:       2475
TITLE:       New EXECUTE STATEMENT implementation asserts or throws an error if used both before and after commin/rollback retaining
DESCRIPTION:
JIRA:        CORE-2038
FBTEST:      bugs.core_2038
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """-- set transaction read write snapshot;
set term ^ ;
execute block returns (i integer)
as
begin
  execute statement 'select 1 from rdb$database' into :i;
end ^
commit retain^
execute block returns (i integer)
as
begin
  execute statement 'select 1 from rdb$database' into :i;
end ^
commit retain^
commit ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
