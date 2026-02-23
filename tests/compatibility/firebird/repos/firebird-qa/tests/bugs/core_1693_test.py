#coding:utf-8

"""
ID:          issue-2118
ISSUE:       2118
TITLE:       Error in EXECUTE STATEMENT inside CONNECT / TRANSACTION START triggers
DESCRIPTION:
JIRA:        CORE-1693
FBTEST:      bugs.core_1693
"""

import pytest
from firebird.qa import *

init_script = """set term ^ ;

create trigger t_connect on connect
as
  declare v integer;
begin
 execute statement 'select 1 from rdb$database' into v;
end ^

set term ; ^
"""

db = db_factory(init=init_script)

test_script = """select 1 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

