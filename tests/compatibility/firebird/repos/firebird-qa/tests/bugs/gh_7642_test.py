#coding:utf-8

"""
ID:          issue-7642
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7642
TITLE:       Getting the current DECFLOAT ROUND/TRAPS settings
DESCRIPTION:
NOTES:
    Checked on 5.0.0.1087: all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    set list on;
    select
        rdb$get_context('SYSTEM', 'DECFLOAT_ROUND') as current_decfloat_round
       ,rdb$get_context('SYSTEM', 'DECFLOAT_TRAPS') as current_decfloat_traps
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CURRENT_DECFLOAT_ROUND          HALF_UP
    CURRENT_DECFLOAT_TRAPS          Division_by_zero,Invalid_operation,Overflow
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
