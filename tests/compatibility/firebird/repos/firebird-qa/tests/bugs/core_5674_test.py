#coding:utf-8

"""
ID:          issue-5940
ISSUE:       5940
TITLE:       Allow unused Common Table Expressions
DESCRIPTION:
JIRA:        CORE-5674
FBTEST:      bugs.core_5674
NOTES:
    [19.07.2023] pzotov
        Adjusted expected error text for FB 4.x and 5.x: it now contains not only errors but also warnings about non-used CTEs.
    [12.08.2023] pzotov
        Adjusted expected error text for FB 3.0.12: now it is the same as for FB 4.x+
        Change caused by commit "Print warnings occurred during commit", date: 07-jul-2023, started on builds 4.0.3.2958 and 5.0.0.1101.
        Discussed with Vlad, 10-jul-2023.
    [02.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- Should PASS but two warnings will be issued:
    -- SQL warning code = -104
    -- -CTE "X" is not used in query
    -- -CTE "Y" is not used in query
    with
    x as(
      select 1 i from rdb$database
    )
    ,y as(
      select i from x
    )
    ,z as (
      select 2 z from rdb$database
    )
    select z y, z*2 x from z
    ;


    -- Should PASS but one warning will be issued:
    -- SQL warning code = -104
    -- -CTE "B" is not used in query
    with
    a as(
      select 0 a from rdb$database
    )
    ,b as(
      select 1 x from c rows 1
    )
    ,c as(
      select 2 x from d rows 1
    )
    ,d as(
      select 3 x from e rows 1
    )
    ,e as(
      select a x from a rows 1
    )
    select * from e
    -- union all select * from b
    ;


    -- Should FAIL with:
    -- Statement failed, SQLSTATE = 42S02
    -- Dynamic SQL Error
    -- -SQL error code = -204
    -- -Table unknown
    -- -FOO
    with recursive
    a as(
      select 0 a from rdb$database
      union all
      select a+1 from a where a.a < 1
    )
    ,b as(
      select 1 a from foo rows 1
    )
    ,c as(
      select 2 b from bar rows 1
    )
    select * from a;

    -- Should FAIL with:
    -- Statement failed, SQLSTATE = 42000
    -- Dynamic SQL Error
    -- -SQL error code = -104
    -- -CTE 'C' has cyclic dependencies
    with recursive
    a as(
      select 0 a from rdb$database
      union all
      select a+1 from a where a.a < 1
    )
    ,b as(
      select 1 a from c rows 1
    )
    ,c as(
      select 2 b from b rows 1
    )
    select * from a;


"""

substitutions = [ ('[ \t]+', ' '), ('(-)?At line \\d+.*', '') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Y 2
    X 4

    X 0
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    if act.is_version('<3.0.12'):
        act.expected_stderr = """
            SQL warning code = -104
            -CTE "X" is not used in query
            -CTE "Y" is not used in query

            SQL warning code = -104
            -CTE "B" is not used in query
            -CTE "C" is not used in query
            -CTE "D" is not used in query

            Statement failed, SQLSTATE = 42S02
            Dynamic SQL Error
            -SQL error code = -204
            -Table unknown
            -FOO
            -At line: column:

            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -CTE 'C' has cyclic dependencies
        """
    elif act.is_version('<6'):
        act.expected_stderr = """
            SQL warning code = -104
            -CTE "X" is not used in query
            -CTE "Y" is not used in query

            SQL warning code = -104
            -CTE "B" is not used in query
            -CTE "C" is not used in query
            -CTE "D" is not used in query

            Statement failed, SQLSTATE = 42S02
            Dynamic SQL Error
            -SQL error code = -204
            -Table unknown
            -FOO
            -At line 8, column 23
            SQL warning code = -104
            -CTE "B" is not used in query
            -CTE "C" is not used in query

            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -CTE 'C' has cyclic dependencies
            SQL warning code = -104
            -CTE "B" is not used in query
            -CTE "C" is not used in query
        """
    else:
        act.expected_stderr = """
            SQL warning code = -104
            -CTE "X" is not used in query
            -CTE "Y" is not used in query
            SQL warning code = -104
            -CTE "B" is not used in query
            -CTE "C" is not used in query
            -CTE "D" is not used in query

            Statement failed, SQLSTATE = 42S02
            Dynamic SQL Error
            -SQL error code = -204
            -Table unknown
            -"FOO"
            SQL warning code = -104
            -CTE "B" is not used in query
            -CTE "C" is not used in query

            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -CTE '"C"' has cyclic dependencies
            SQL warning code = -104
            -CTE "B" is not used in query
            -CTE "C" is not used in query
        """

    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
