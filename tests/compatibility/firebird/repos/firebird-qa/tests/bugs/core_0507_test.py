#coding:utf-8

"""
ID:          issue-468
ISSUE:       468
TITLE:       Ambiguous statements return unpredictable results
DESCRIPTION:
JIRA:        CORE-517
FBTEST:      bugs.core_0507
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    select r.rdb$relation_name, rc.rdb$relation_name, rc.rdb$constraint_type
    from rdb$relations r left join rdb$relation_constraints rc
    on r.rdb$relation_name = rc.rdb$relation_name
    order by rdb$relation_name;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42702
    Dynamic SQL Error
    -SQL error code = -204
    -Ambiguous field name between a field and a field in the select list with name
    -RDB$RELATION_NAME
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

