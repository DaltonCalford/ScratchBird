#coding:utf-8

"""
ID:          issue-4300
ISSUE:       4300
TITLE:       subselect with reference to outer select fails
DESCRIPTION:
JIRA:        CORE-3967
FBTEST:      bugs.core_3967
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='employee-ods12.fbk')

test_script = """
    set list on;
    select r1.rdb$relation_name rel_name
    from rdb$relation_fields f1
    join rdb$relations r1 on f1.rdb$relation_name = r1.rdb$relation_name
    where
        f1.rdb$field_name = upper('emp_no') and
        not exists(
            select r2.rdb$relation_name
            from rdb$relation_fields f2
            join rdb$relations r2 on f2.rdb$relation_name=r1.rdb$relation_name
            where f2.rdb$field_name = upper('phone_ext')
        )
    order by 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    REL_NAME                        EMPLOYEE_PROJECT
    REL_NAME                        SALARY_HISTORY
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

