#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8836
TITLE:       Regression: Aggregate function ANY_VALUE always returns NULL
DESCRIPTION:
    doc/sql.extensions/README.aggregate_functions.md
    ANY_VALUE is a non-deterministic aggregate function that returns its expression for an arbitrary record from the grouped rows.
    NULLs are ignored. It's returned only in the case of none evaluated records having a non-null value.
NOTES:
    [04.01.2026] pzotov
    Confirmed regression on 6.0.0.1377.
    Checked on 6.0.0.1385.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """

    recreate table employee(
       emp_no int primary key using index emp_pk
      ,dep_no int
      ,first_name varchar(10)
    );
    insert into employee select i, mod(i, 5), 'name #' || i from (select row_number()over() i from rdb$types rows 10);
    insert into employee select -i, -mod(i, 5), null from (select row_number()over() i from rdb$types rows 10);
    commit;

    set list on;
    select
      emp_no,
      min(first_name) as first_name_min,
      any_value(first_name) is null as any_value_is_null
    from employee
    where emp_no >= 3
    group by emp_no
    order by 1
    fetch first 7 rows only;

    select
      dep_no,
      min(first_name) as first_name_min,
      any_value(first_name) is null as any_value_is_null
    from employee
    where dep_no < 3
    group by dep_no
    order by 1
    fetch first 7 rows only;
"""

substitutions = [('[=]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    EMP_NO                          3
    FIRST_NAME_MIN                  name #3
    ANY_VALUE_IS_NULL               <false>
    EMP_NO                          4
    FIRST_NAME_MIN                  name #4
    ANY_VALUE_IS_NULL               <false>
    EMP_NO                          5
    FIRST_NAME_MIN                  name #5
    ANY_VALUE_IS_NULL               <false>
    EMP_NO                          6
    FIRST_NAME_MIN                  name #6
    ANY_VALUE_IS_NULL               <false>
    EMP_NO                          7
    FIRST_NAME_MIN                  name #7
    ANY_VALUE_IS_NULL               <false>
    EMP_NO                          8
    FIRST_NAME_MIN                  name #8
    ANY_VALUE_IS_NULL               <false>
    EMP_NO                          9
    FIRST_NAME_MIN                  name #9
    ANY_VALUE_IS_NULL               <false>
    DEP_NO                          -4
    FIRST_NAME_MIN                  <null>
    ANY_VALUE_IS_NULL               <true>
    
    DEP_NO                          -3
    FIRST_NAME_MIN                  <null>
    ANY_VALUE_IS_NULL               <true>
    DEP_NO                          -2
    FIRST_NAME_MIN                  <null>
    ANY_VALUE_IS_NULL               <true>
    DEP_NO                          -1
    FIRST_NAME_MIN                  <null>
    ANY_VALUE_IS_NULL               <true>
    DEP_NO                          0
    FIRST_NAME_MIN                  name #10
    ANY_VALUE_IS_NULL               <false>
    DEP_NO                          1
    FIRST_NAME_MIN                  name #1
    ANY_VALUE_IS_NULL               <false>
    DEP_NO                          2
    FIRST_NAME_MIN                  name #2
    ANY_VALUE_IS_NULL               <false>
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

