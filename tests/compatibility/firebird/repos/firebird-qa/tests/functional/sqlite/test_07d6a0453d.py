#coding:utf-8

"""
ID:          07d6a0453d
ISSUE:       https://www.sqlite.org/src/tktview/07d6a0453d
TITLE:       OFFSET ignored if there is no FROM clause
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select 1 a, 2 b, 3 c
    from rdb$database
    offset 25 rows
    fetch first row only;

    select 1 a, 2 b, 3 c
    from rdb$database
    offset 0 rows
    fetch first row only;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0

    A 1
    B 2
    C 3
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
