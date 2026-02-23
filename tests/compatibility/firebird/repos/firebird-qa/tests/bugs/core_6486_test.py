#coding:utf-8

"""
ID:          issue-6716
ISSUE:       6716
TITLE:       FETCH RELATIVE has an off by one error for the first row
DESCRIPTION:
JIRA:        CORE-6486
FBTEST:      bugs.core_6486
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns(id_fetch_rel1 int, rc_fetch_rel1 int) as
        declare c scroll cursor for
        (
            select  1 id from rdb$database union all
            select  2 id from rdb$database union all
            select  3 id from rdb$database
        )
        ;
    begin
        open c;
        fetch relative 1 from c;
        id_fetch_rel1 = c.id;
        rc_fetch_rel1 = row_count;
        suspend;
        close c;
    end
    ^

    execute block returns(id_fetch_next int, rc_fetch_next int) as
        declare c scroll cursor for
        (
            select  1 id from rdb$database union all
            select  2 id from rdb$database union all
            select  3 id from rdb$database
        )
        ;
    begin
        open c;
        fetch next from c;
        id_fetch_next = c.id;
        rc_fetch_next = row_count;
        suspend;
        close c;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID_FETCH_REL1                   1
    RC_FETCH_REL1                   1
    ID_FETCH_NEXT                   1
    RC_FETCH_NEXT                   1
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
