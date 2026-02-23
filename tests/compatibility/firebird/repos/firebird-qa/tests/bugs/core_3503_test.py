#coding:utf-8

"""
ID:          issue-3861
ISSUE:       3861
TITLE:        ALTER VIEW crashes the server if the new version has an artificial
  (aggregate or union) stream at the position of a regular context in the older version
DESCRIPTION:
JIRA:        CORE-3503
FBTEST:      bugs.core_3503
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3503.fbk')

test_script = """
    create or alter view v_test (id)
    as
    select rdb$relation_id from rdb$relations
    union all
    select rdb$relation_id from rdb$relations;
    commit; -- here the crash happens
    set list on;
    select (select count(id) from v_test) / count(*) c
    from rdb$relations;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    C                               2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

