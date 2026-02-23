#coding:utf-8

"""
ID:          issue-4412
ISSUE:       4412
TITLE:       Regression: Group by fails if subselect-column is involved
DESCRIPTION:
JIRA:        CORE-4084
FBTEST:      bugs.core_4084
NOTES:
    [28.06.2025] pzotov
    Data in STDOUT is irrelevant and may differ in among FB versions.
    Only STDERR must be checked in this test.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    select
        iif(d is null, 10, 0) + sys as sys,
        count(*)
    from (
        select
            ( select d.rdb$relation_id from rdb$database d ) as d,
            coalesce(r.rdb$system_flag, 0) as sys
        from rdb$relations r
    )
    group by 1;
"""

act = isql_act('db', test_script)

expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute(combine_output = False) # ::: NB ::: Only STDERR is checked in this test!
    assert act.clean_stderr == act.clean_expected_stderr

