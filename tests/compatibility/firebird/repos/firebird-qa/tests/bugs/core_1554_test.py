#coding:utf-8

"""
ID:          issue-1971
ISSUE:       1971
TITLE:       select ... where ... <> ALL (select ... join ...) bug
DESCRIPTION:
JIRA:        CORE-1554
FBTEST:      bugs.core_1554
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
    (
        select count(*) from rdb$triggers t1
    )
    -
    (
        select count(*)
        from rdb$triggers t1
        where
            t1.RDB$SYSTEM_FLAG=1 and
            t1.rdb$trigger_name <>
            all (
                select t2.rdb$trigger_name
                from rdb$triggers t2
                join rdb$triggers t3 on t3.rdb$trigger_name=t2.rdb$trigger_name
                where t2.rdb$trigger_name='xxx'
            )
    ) as cnt
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT                             0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

