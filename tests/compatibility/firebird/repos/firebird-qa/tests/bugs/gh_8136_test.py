#coding:utf-8

"""
ID:          issue-8136
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8136
TITLE:       Server crashes with IN (dbkey1, dbkey2, ...) condition
DESCRIPTION:
NOTES:
    [28.05.2024] pzotov
    Confirmed crash on 6.0.0.36, 5.0.1.1408
    Checked on 6.0.0.363-40d0b41, 5.0.1.1408-c432bd0

    [09.06.2024] pzotov
    Added temporary mark 'disabled_in_forks' to SKIP this test when QA verifies *fork* rather than standard FB.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select 1 from rdb$database where rdb$db_key in (MAKE_DBKEY(1, 0), MAKE_DBKEY(1, 1));
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    1
"""

@pytest.mark.disabled_in_forks
@pytest.mark.version('>=5.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
