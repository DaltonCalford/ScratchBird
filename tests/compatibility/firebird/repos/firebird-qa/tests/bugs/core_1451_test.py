#coding:utf-8

"""
ID:          issue-1869
ISSUE:       1869
TITLE:       Using RDB$DB_KEY in where section while selecting from a procedure crashes the server
DESCRIPTION:
JIRA:        CORE-1451
FBTEST:      bugs.core_1451
"""

import pytest
from firebird.qa import *

init_script = """SET TERM ^;
create procedure test_proc
returns (A INTEGER)
as
begin
  A = 1;
  SUSPEND;
end^
SET TERM ;^
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """select * from test_proc
where rdb$db_key is not null;
"""

act = isql_act('db', test_script,
               substitutions=[('line\\s[0-9]+,', 'line x'),
                              ('column\\s[0-9]+', 'column y')])

expected_stderr = """Statement failed, SQLSTATE = 42S22
Dynamic SQL Error
-SQL error code = -206
-Column unknown
-DB_KEY
-At line 2, column 7
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

