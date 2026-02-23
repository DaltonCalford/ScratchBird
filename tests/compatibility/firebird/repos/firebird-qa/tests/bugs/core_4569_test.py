#coding:utf-8

"""
ID:          issue-4886
ISSUE:       4886
TITLE:       BIN_SHL and BIN_SHR does not work in Dialect 1
DESCRIPTION:
JIRA:        CORE-4569
FBTEST:      bugs.core_4569
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    set list on;
    select bin_shl(1073741824, 2) bin_shl from rdb$database
    union all
    select bin_shl(1, 32) from rdb$database
    union all
    select bin_shl(0, 1) from rdb$database
    union all
    select bin_shl(-1073741824, 2) from rdb$database
    union all
    select bin_shl(-1, 32) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    BIN_SHL                         4294967296
    BIN_SHL                         4294967296
    BIN_SHL                         0
    BIN_SHL                         -4294967296
    BIN_SHL                         -4294967296
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

