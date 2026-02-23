#coding:utf-8

"""
ID:          issue-6693
ISSUE:       6693
TITLE:       Incorrect query result when using named window
DESCRIPTION:
JIRA:        CORE-6460
FBTEST:      bugs.core_6460
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test (
     emp_no smallint,
     dept_no char(3),
     salary numeric(10,2)
    );
    commit;

    insert into test (emp_no, dept_no, salary) values ( 85, 'd01', 99999);
    insert into test (emp_no, dept_no, salary) values (127, 'd01', 11111);
    commit;

    select e.emp_no, e.dept_no, e.salary,
           last_value(e.salary) over (order by e.salary, e.emp_no) as last_2,
           last_value(e.salary) over w2 as last_w2,
           last_value(e.salary) over (order by e.salary, e.emp_no RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) as last_3,
           last_value(e.salary) over w3 as last_w3,
           last_value(e.salary) over w4 as last_w4
      from test e
      window
          w1 as (),
          w2 as (w1 order by e.salary, e.emp_no),
          w3 as (w1 order by e.salary, e.emp_no RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING),
          w4 as (w2 RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING)
    order by e.emp_no
    ;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
 EMP_NO DEPT_NO                SALARY                LAST_2               LAST_W2                LAST_3               LAST_W3               LAST_W4
     85 d01                  99999.00              99999.00              99999.00              99999.00              99999.00              99999.00
    127 d01                  11111.00              11111.00              11111.00              99999.00              99999.00              99999.00
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
