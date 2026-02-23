#coding:utf-8

"""
ID:          issue-2211
ISSUE:       2211
TITLE:       Error with EXECUTE PROCEDURE inside EXECUTE STATEMENT
DESCRIPTION:
JIRA:        CORE-1784
FBTEST:      bugs.core_1784
"""

import pytest
from firebird.qa import *

init_script = """set term ^ ;
create procedure p1 returns (n1 integer, n2 integer)
as
begin
    n1 = 111;
    n2 = 222;
end ^

set term ; ^

"""

db = db_factory(init=init_script)

test_script = """set term ^ ;

execute block returns (n1 integer, n2 integer)
as
begin
  execute statement
    'execute procedure p1' into n1, n2;
  suspend;
end^

set term ; ^
"""

act = isql_act('db', test_script)

expected_stdout = """
          N1           N2
============ ============
         111          222

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

