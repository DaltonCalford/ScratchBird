#coding:utf-8

"""
ID:          issue-2026
ISSUE:       2026
TITLE:       Bugcheck 232 (invalid operation) for an aggregated query
DESCRIPTION:
JIRA:        CORE-1605
FBTEST:      bugs.core_1605
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test (id int);

    set term ^;
    create or alter procedure sp_test (id int) returns (result int) as
    begin
      result = id * id;
      suspend;
    end
    ^

    set term ;^
    commit;

    insert into test values(1);
    insert into test values(2);
    insert into test values(3);
    commit;

    select
        sum( id ),
        sum( (select result from sp_test(id)) )
    from test
    group by 2;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Cannot use an aggregate or window function in a GROUP BY clause
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

