#coding:utf-8

"""
ID:          issue-5448
ISSUE:       5448
TITLE:       HAVING COUNT(*) NOT IN ( <Q> ) prevent record from appearing in outer resultset
  when it should be there (<Q> = resultset without nulls)
DESCRIPTION:
JIRA:        CORE-5165
FBTEST:      bugs.core_5165
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed proper result on: WI-V3.0.0.32418, WI-T4.0.0.98
    set list on;
    set count on;
    select 1 as check_ok
    from rdb$database r
    group by r.rdb$relation_id
    having count(*) not in (select -1 from rdb$database r2);

    select 2 as check_ok
    from rdb$database r
    group by r.rdb$relation_id
    having count(1) not in (select -1 from rdb$database r2);
"""

act = isql_act('db', test_script)

expected_stdout = """
    CHECK_OK                        1
    Records affected: 1

    CHECK_OK                        2
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

