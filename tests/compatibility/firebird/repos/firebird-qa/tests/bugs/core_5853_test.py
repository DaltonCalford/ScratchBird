#coding:utf-8

"""
ID:          issue-6113
ISSUE:       6113
TITLE:       Forward-compatible expressions LOCALTIME and LOCALTIMESTAMP
DESCRIPTION:
JIRA:        CORE-5853
FBTEST:      bugs.core_5853
NOTES:
    [03.07.2025] pzotov
    Added substitution to suppress all except sqltype and fields name from SQLDA output.
    Checked on 6.0.0.892; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

test_script = """
    set list on;
    set sqlda_display on;
    select localtime from rdb$database;
    select localtimestamp from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype:|name:).)*$',''),('[ \t]+',' ')])

expected_stdout = """
    01: sqltype: 560 TIME scale: 0 subtype: 0 len: 4
    : name: LOCALTIME alias: LOCALTIME
    01: sqltype: 510 TIMESTAMP scale: 0 subtype: 0 len: 8
    : name: LOCALTIMESTAMP alias: LOCALTIMESTAMP
"""

@pytest.mark.version('>=3.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
