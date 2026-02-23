#coding:utf-8

"""
ID:          issue-8619
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8619
TITLE:       Regression in 6.0.0.653 ("Stack overflow. ... requirements of the runtime stack have exceeded the memory").
DESCRIPTION:
NOTES:
    [22.06.2025] pzotov
    Checked on 6.0.0.853-c1954c4
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(id1 int, id2 int, id3 int, unique(id1, id2, id3));

    -- #######################
    -- INSERT USING SELECT ...
    -- #######################
    insert into test(id1, id2, id3)
    with
    a as (
        select 1000 as v from rdb$database union all
        select 1000 as v from rdb$database union all
        select null as v from rdb$database
    )
    select distinct a1.v, a2.v, a3.v
    from a a1
    cross join a a2
    cross join a a3
    ;

    select 1 from mon$database;
"""

act = isql_act('db', test_script, substitutions = [('[\t ]+', ' ')])

expected_stdout = """
    CONSTANT 1 
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
