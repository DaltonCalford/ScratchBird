#coding:utf-8

"""
ID:          issue-2459
ISSUE:       2459
TITLE:       "EXECUTE BLOCK" statement does not support "CREATE USER"
DESCRIPTION:
JIRA:        CORE-2022
FBTEST:      bugs.core_2022
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """set term ^ ;
execute block
as
begin
EXECUTE statement 'create user test1 password ''test1''';
EXECUTE statement 'create user test2 password ''test2''';
end ^

commit ^

execute block
as
begin
EXECUTE statement 'drop user test1';
EXECUTE statement 'drop user test2';
end ^

commit ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
