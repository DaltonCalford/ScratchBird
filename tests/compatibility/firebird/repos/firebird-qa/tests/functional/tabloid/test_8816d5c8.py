#coding:utf-8

"""
ID:          issue-8816d5c8
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/adbbcc064f7c18e49ebb5ce3ff91308e8816d5c8
TITLE:       Fixed crash when IN predicate is delivered into aggregates/unions 
DESCRIPTION:
NOTES:
    [13.09.2023] pzotov
    Bug seems to be kind of regression: no crash on old FB snapshots, e.g. 5.0.0.812

    Confirmed crash on 5.0.0.1200.
    Checked on 5.0.0.1204
    Thanks to dimitr for provided queries.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    recreate table employee(emp_no int);
    insert into employee(emp_no) values(2);
    insert into employee(emp_no) values(4);
    insert into employee(emp_no) values(5);
    commit;

    select * from (
       select emp_no from employee
       union all
       select emp_no from employee
    ) where emp_no in (1,2,3);

    select * from (
       select emp_no from employee group by 1
    ) where emp_no in (1,2,3);
"""

act = isql_act('db', test_script)

expected_stdout = """
    2
    2
    2
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
