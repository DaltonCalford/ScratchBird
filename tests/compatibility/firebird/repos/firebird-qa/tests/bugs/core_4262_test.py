#coding:utf-8

"""
ID:          issue-4586
ISSUE:       4586
TITLE:       Context parsing error with derived tables and CASE functions
DESCRIPTION:
JIRA:        CORE-4262
FBTEST:      bugs.core_4262
NOTES:
    [29.06.2025] pzotov
    Data in STDOUT has no matter.
    Only STDERR must be checked in this test.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
set list on;
select
    case
        when col = 1 then 'y'
        when col = 0 then 'n'
    end 
    as text
from (
    select case when exists (select 1 from rdb$database ) then 1 else 0 end as col
    from rdb$relations
);

select col as col1, col as col2
from (
    select case when exists (select 1 from rdb$database ) then 1 else 0 end as col
    from rdb$relations
);
"""

act = isql_act('db', test_script)

expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute(combine_output = False) # ::: NB ::: we have to check only EMPTY stderr here!
    assert act.clean_stderr == act.clean_expected_stderr
