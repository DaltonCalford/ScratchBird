#coding:utf-8

"""
ID:          issue-1607
ISSUE:       1607
TITLE:       Union returns inconsistent field names
DESCRIPTION:
JIRA:        CORE-1181
FBTEST:      bugs.core_1181
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test1(id numeric(15, 2));
    commit;

    recreate table test2(id double precision);
    commit;


    insert into test1 values(0);
    commit;

    insert into test2 values(1e0); ---- do NOT use 0e0! something weird occurs on linux: aux '0' in fractional part!
    commit;


    set list on;

    select id as test1_id
    from test1
    group by id

    union

    select cast(0 as numeric(15,2))
    from rdb$database;

    -----------------------------------

    select id as test2_id
    from test2
    group by id

    union

    select cast(1 as double precision)
    from rdb$database;


    -- Results were checked both on dialect 1 & 3, they are identical.
"""

act = isql_act('db', test_script)

expected_stdout = """
    TEST1_ID                        0.00
    TEST2_ID                        1.000000000000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

