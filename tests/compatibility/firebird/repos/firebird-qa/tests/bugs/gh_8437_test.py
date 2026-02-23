#coding:utf-8

"""
ID:          issue-8437
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8437
TITLE:       Segmentation fault when running query with partition by and subquery
DESCRIPTION:
NOTES:
    [24.02.2025] pzotov
    Confirmed bug on 6.0.0.647-9fccb55; 5.0.3.1619-81c5f17; 4.0.6.3188-8ee1ca8
    Checked on 6.0.0.652-226622f; 5.0.3.1622-c1a518f; 4.0.6.3189-3fb0bbf - all fine.

    [18.03.2025] pzotov
    Confirmed crash on 3.0.13.33798; checked on 3.0.13.33804 after fix #e02eaffb.
    Reduced min_version from 4.0.6 to 3.0.13.

    [25.04.2024] pzotov
    Added temporary mark 'disabled_in_forks' to SKIP this test when QA runs agains *fork* of standard FB.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select row_number()over(partition by (select 1 from rdb$database)) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    1
"""

@pytest.mark.disabled_in_forks
@pytest.mark.version('>=3.0.13')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

