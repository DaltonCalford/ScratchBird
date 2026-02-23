#coding:utf-8

"""
ID:          issue-2400
ISSUE:       2400
TITLE:       Incorrect extraction of MILLISECONDs
DESCRIPTION:
JIRA:        CORE-1962
FBTEST:      bugs.core_1962
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select extract(millisecond from time '01:00:10.5555') EXTRACTED_MS from rdb$database
    union all
    select extract(millisecond from time '00:00:00.0004') from rdb$database
    union all
    select extract(millisecond from time '23:59:59.9995') from rdb$database
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    EXTRACTED_MS                    555.5
    EXTRACTED_MS                    0.4
    EXTRACTED_MS                    999.5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

