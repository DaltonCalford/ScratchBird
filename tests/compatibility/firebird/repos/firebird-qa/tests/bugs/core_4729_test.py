#coding:utf-8

"""
ID:          issue-5035
ISSUE:       5035
TITLE:       Add a flag to mon$database helping to decide what type of security database
  is used - default, self or other
DESCRIPTION:
JIRA:        CORE-4729
FBTEST:      bugs.core_4729
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='ozzy', password='osb')

test_script = """
    set wng off;
    set list on;

    -- Check that info can be seen by SYSDBA:
    select current_user,mon$sec_database from mon$database;
    commit;

    -- Check that info can be seen by non-privileged user:
    connect '$(DSN)' user ozzy password 'osb';
    select current_user,mon$sec_database from mon$database;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    USER                            SYSDBA
    MON$SEC_DATABASE                Default
    USER                            OZZY
    MON$SEC_DATABASE                Default
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

