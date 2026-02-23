#coding:utf-8

"""
ID:          issue-8087
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8087
TITLE:       AV when preparing a query with IN list that contains both literals and sub-query
DESCRIPTION:
NOTES:
    [19.04.2024] pzotov
    Reduced min_version to 5.0.1 after backporting (commit #0e9ef69).
    Confirmed bug (AV) on 6.0.0.315.
    Checked on 6.0.0.321 #1d96c10, 5.0.1.1383 #0e9ef69 (intermediate snapshot) - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set count on;
    select 1
    from rdb$relations r
    where r.rdb$relation_id in (1, (select d.rdb$relation_id from rdb$database d))
    rows 0
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=5.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
