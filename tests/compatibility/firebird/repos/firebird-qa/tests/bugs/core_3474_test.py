#coding:utf-8

"""
ID:          issue-3835
ISSUE:       3835
TITLE:       Regression in joins on procedures
DESCRIPTION:
JIRA:        CORE-3474
FBTEST:      bugs.core_3474
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='employee-ods12.fbk')

test_script = """
    set list on;
    select e.emp_no emp_1, e.last_name name_1, p.proj_name proj_1
    from employee e
    left join
        ( get_emp_proj(e.emp_no) proc
          join project p on p.proj_id = proc.proj_id
        ) on 1=1
    order by 1,2,3
    rows 1;

    select e.emp_no emp_2, e.last_name name_2, p.proj_name proj_2
    from
        (
            employee e
            left join get_emp_proj(e.emp_no) proc on 1=1
        )
        left join project p on p.proj_id = proc.proj_id
    order by 1,2,3
    rows 1;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('(-)?At line.*', '')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -E.EMP_NO
    EMP_2                           2
    NAME_2                          Nelson
    PROJ_2                          <null>
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"E"."EMP_NO"
    EMP_2                           2
    NAME_2                          Nelson
    PROJ_2                          <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
