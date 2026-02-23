#coding:utf-8

"""
ID:          issue-7467
ISSUE:       7467
TITLE:       Simple SQL crashes firebird: select cast(rdb$db_key as integer) from rdb$database
NOTES:
    [14.02.2023] pzotov
    Confirmed crash on 5.0.0.823; 4.0.3.2894; 3.0.10.33657
    Checked on 5.0.0.932; 4.0.3.2900; 3.0.11.33664 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    select cast(rdb$db_key as integer) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "DBKEY"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
