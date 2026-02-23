#coding:utf-8

"""
ID:          issue-4042
ISSUE:       4042
TITLE:       internal Firebird consistency check in query with grouping by subquery+stored procedure+aggregate
DESCRIPTION:
JIRA:        CORE-3694
FBTEST:      bugs.core_3694
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- As of 27-apr-2015,, this error exists at least up to WI-V2.5.5.26861
    -- (not only in 2.5.1 as it is issued in the ticket).
    set term ^;
    create or alter procedure dummy_proc(val integer) returns(result integer) as
    begin
      result = val;
      suspend;
    end
    ^
    set term ;^
    commit;

    -- wrong query (uses an aggregate function in a group by clause)
    select ( select result from dummy_proc(sum(t.rdb$type)) ) as is_correct
          ,count(*) from rdb$types t
    group by is_correct;
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

