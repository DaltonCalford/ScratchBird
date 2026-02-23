#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8820
TITLE:       Missing a column name for boolean expression
DESCRIPTION:
NOTES:
    [05.12.2025] pzotov
    There is at least one ticket with similar issues:
    https://github.com/FirebirdSQL/firebird/issues/8543
    (ISQL does not show 'CONSTANT' for ... if UNION presents in the query)

    Checked on 6.0.0.1364-9048844.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    set list OFF;
    select (1 not in (1, null, 3)) from rdb$database;

    select (1 = 2) from rdb$database;

    select 2 < 3 from rdb$database;

    select null is not distinct from null from rdb$database; 

    select exists(select 1 from rdb$database) from rdb$database; 

    select singular(select 1 from rdb$database) from rdb$database; 

    select 1 = any (select 1 from rdb$database) from rdb$database; 

"""

substitutions = [('[=]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    BOOL
    <false>
    BOOL
    <false>
    BOOL
    <true>
    BOOL
    <true>
    BOOL
    <true>
    BOOL
    <true>
    BOOL
    <true>
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

