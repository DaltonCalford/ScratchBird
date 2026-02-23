#coding:utf-8

"""
ID:          issue-3470
ISSUE:       3470
TITLE:       Built-in function POWER(X, Y) does not work when the X argument is negative
  and the Y value is scaled numeric but integral
DESCRIPTION:
JIRA:        CORE-3091
FBTEST:      bugs.core_3091
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select 'power( 3, 2 )', power( 3, 2 ) from RDB$DATABASE
union all
select 'power( -3, 2 )', power( -3, 2 ) from RDB$DATABASE
union all
select 'power( 3, -2 )', power( 3, -2 ) from RDB$DATABASE
union all
select 'power( -3, -2 )', power( -3, -2 ) from RDB$DATABASE
union all
select 'power( 3, 2.0 )', power( 3, 2.0 ) from RDB$DATABASE
union all
select 'power( 3, -2.0 )', power( 3, -2.0 ) from RDB$DATABASE
union all
select 'power( -3, 2.0 )', power( -3, 2.0 ) from RDB$DATABASE
union all
select 'power( -3, -2.0 )', power( -3, -2.0 ) from RDB$DATABASE
union all
select 'power( -3.0, 2 )', power( -3.0, 2 ) from RDB$DATABASE
union all
select 'power( -3.0, 2.0 )', power( -3.0, 2.0 ) from RDB$DATABASE
union all
select 'power( -3.0, -2.0 )', power( -3.0, -2.0 ) from RDB$DATABASE
union all
select 'power( 3.0, -2.0 )', power( 3.0, -2.0 ) from RDB$DATABASE
union all
select 'power( 3.0, 2.0 )', power( 3.0, 2.0 ) from RDB$DATABASE
union all
select 'power( 3.0, 2 )', power( 3.0, 2 ) from RDB$DATABASE
union all
select 'power( 3.0, -2 )', power( 3.0, -2 ) from RDB$DATABASE
;"""

act = isql_act('db', test_script)

expected_stdout = """

=================== =======================
power( 3, 2 )             9.000000000000000
power( -3, 2 )            9.000000000000000
power( 3, -2 )           0.1111111111111111
power( -3, -2 )          0.1111111111111111
power( 3, 2.0 )           9.000000000000000
power( 3, -2.0 )         0.1111111111111111
power( -3, 2.0 )          9.000000000000000
power( -3, -2.0 )        0.1111111111111111
power( -3.0, 2 )          9.000000000000000
power( -3.0, 2.0 )        9.000000000000000
power( -3.0, -2.0 )      0.1111111111111111
power( 3.0, -2.0 )       0.1111111111111111
power( 3.0, 2.0 )         9.000000000000000
power( 3.0, 2 )           9.000000000000000
power( 3.0, -2 )         0.1111111111111111

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

