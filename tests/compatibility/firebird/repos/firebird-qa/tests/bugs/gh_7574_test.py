#coding:utf-8

"""
ID:          issue-7574
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7574
TITLE:       Derived table syntax allows dangling "AS"
NOTES:
    [17.05.2023] pzotov
    Confirmed problem on 5.0.0.1049. Checked on 5.0.0.1050 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    -- All following statements must FAIL since 5.0.0.1050:
    select i from (select 1 as i from rdb$database) as;
    -------------------------------------
    with
    a as 
    (select 2 as i from rdb$database)
    select * from (select i from a) as
    ;
    -------------------------------------
    with recursive
    r as (
         select 0 as i from rdb$database
         UNION ALL
         select r.i+1 as i from r where r.i < 3
    )
    select * from (select i from r) as
    ;
"""

act = isql_act('db', test_script, substitutions = [('line \\d+, column \\d+', '')])

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command - line 1, column 49
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command - line 4, column 37
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command - line 7, column 37
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
