#coding:utf-8

"""
ID:          issue-1474
ISSUE:       1474
TITLE:       Wrong parameter matching for self-referenced procedures
DESCRIPTION:
JIRA:        CORE-1055
FBTEST:      bugs.core_1055
"""

import pytest
from firebird.qa import *

init_script = """SET TERM ^;

create procedure PN (p1 int)
as
begin
  execute procedure PN (:p1);
end ^

SET TERM ;^

commit;
"""

db = db_factory(init=init_script)

test_script = """SET TERM ^;

alter procedure PN (p1 int, p2 int)
as
begin
  execute procedure PN (:p1, :p2);
end^

SET TERM ;^

commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
