#coding:utf-8

"""
ID:          issue-1795
ISSUE:       1795
TITLE:       Add an ability to change role without reconnecting to database.
DESCRIPTION:
JIRA:        CORE-1377
FBTEST:      bugs.core_1377
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create role r1377a;
    create role r1377b;
    commit;
    grant r1377a to sysdba;
    grant r1377b to sysdba;
    commit;
    set list on;
    set role r1377a;
    select current_user, current_role from rdb$database;
    set role r1377b;
    select current_user, current_role from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    USER                            SYSDBA
    ROLE                            R1377A

    USER                            SYSDBA
    ROLE                            R1377B
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

