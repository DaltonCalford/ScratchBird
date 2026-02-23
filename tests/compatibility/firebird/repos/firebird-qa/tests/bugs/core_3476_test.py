#coding:utf-8

"""
ID:          issue-3837
ISSUE:       3837
TITLE:       LIST function wrongly concatenates binary blobs
DESCRIPTION:
JIRA:        CORE-3476
FBTEST:      bugs.core_3476
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select ascii_val( left(list(f,''),1) ) v1, ascii_val( right(list(f,''),1) ) v2
    from (
        select cast(ascii_char(0xff) as blob sub_type 0) as f
        from rdb$database
        union all
        select cast(ascii_char(0xde) as blob sub_type 0) as f
        from rdb$database
    );
    -- NB: proper result will be only in 3.0, WI-V2.5.4.26853 produces:
    -- V1                              46
    -- V2                              46
"""

act = isql_act('db', test_script)

expected_stdout = """
    V1                              255
    V2                              222
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

