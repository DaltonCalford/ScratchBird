#coding:utf-8

"""
ID:          issue-3578
ISSUE:       3578
TITLE:       Constraint violation error of CAST is not raised inside views
DESCRIPTION:
JIRA:        CORE-3204
FBTEST:      bugs.core_3204
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter  view v1 as select 1 id from rdb$database;
    commit;
    set term ^;
    execute block as
    begin
      execute statement 'drop domain d1';
    when any do begin end
    end
    ^
    set term ;^
    commit;

    create domain d1 integer not null;
    commit;

    set list on;

    select cast(null as d1) from rdb$database; -- error: ok
    commit;

    create or alter view v1 as select cast(null as d1) x from rdb$database;
    commit;

    select * from v1;

"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    validation error for CAST, value "*** null ***"
    Statement failed, SQLSTATE = 42000
    validation error for CAST, value "*** null ***"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

