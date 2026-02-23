#coding:utf-8

"""
ID:          c0aeea67d5
ISSUE:       https://www.sqlite.org/src/tktview/c0aeea67d5
TITLE:       Incorrect LIKE result
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select '%' like '%' escape '_' as result_1 from rdb$database;
    select '' like '%' escape '_'  as result_2 from rdb$database;
    select '_' like '%' escape '_'  as result_3 from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    RESULT_1 <true>
    RESULT_2 <true>
    RESULT_3 <true>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
