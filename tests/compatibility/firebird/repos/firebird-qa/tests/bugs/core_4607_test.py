#coding:utf-8

"""
ID:          issue-4922
ISSUE:       4922
TITLE:       Add support for having more than one UserManager in firebird.conf and use them from SQL
DESCRIPTION:
NOTES:
[21.10.2021]
  This test requires Legacy_UserManager to be listed in firebird.conf UserManager option,
  which is NOT by default. Otherwise it will FAIL with "Missing requested management plugin"
  Also, it does not use user_factory fixtures as it's the point to create/drop users in test script.
JIRA:        CORE-4607
FBTEST:      bugs.core_4607
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    create view v_test as
    select sec$user_name, sec$plugin
    from sec$users
    where upper(sec$user_name) starting with upper('tmp$c4607')
    order by 1,2
    ;
    commit;
    create or alter user tmp$c4607_leg password '123' using plugin Legacy_UserManager;
    create or alter user tmp$c4607_srp password '456' using plugin Srp;
    commit;
    select * from v_test;
    commit;
    drop user tmp$c4607_leg using plugin Legacy_UserManager;
    drop user tmp$c4607_srp using plugin Srp;
    commit;
    select * from v_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    SEC$USER_NAME                   TMP$C4607_LEG
    SEC$PLUGIN                      Legacy_UserManager

    SEC$USER_NAME                   TMP$C4607_SRP
    SEC$PLUGIN                      Srp

    Records affected: 2

    Records affected: 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

