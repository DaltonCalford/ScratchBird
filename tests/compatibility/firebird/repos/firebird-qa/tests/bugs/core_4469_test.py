#coding:utf-8

"""
ID:          issue-4789
ISSUE:       4789
TITLE:       Add field in SEC$USERS reflecting whether a user has RDB$ADMIN role in security database
DESCRIPTION:
JIRA:        CORE-4469
FBTEST:      bugs.core_4469
"""

import pytest
from firebird.qa import *

db = db_factory()

user_boss1 = user_factory('db', name='boss1', do_not_create=True)
user_boss2 = user_factory('db', name='boss2', do_not_create=True)

test_script = """
    set list on;

    create user boss1 password '123' grant admin role;
    commit;
    select SEC$ADMIN is_admin_boss1 from sec$users where sec$user_name = upper('boss1');


    create user boss2 password '456';
    commit;
    select SEC$ADMIN is_admin_boss2a from sec$users where sec$user_name = upper('boss2');

    alter user boss2 grant admin role;
    commit;
    select SEC$ADMIN is_admin_boss2b from sec$users where sec$user_name = upper('boss2');

    alter user boss2 revoke admin role;
    commit;
    select SEC$ADMIN is_admin_boss2c from sec$users where sec$user_name = upper('boss2');
"""

act = isql_act('db', test_script, substitutions=[('Statement failed, SQLSTATE = HY000', ''),
                                                   ('record not found for user:.*', '')])

expected_stdout = """
    IS_ADMIN_BOSS1                  <true>
    IS_ADMIN_BOSS2A                 <false>
    IS_ADMIN_BOSS2B                 <true>
    IS_ADMIN_BOSS2C                 <false>
"""

expected_stderr = """
Statement failed, SQLSTATE = HY000
record not found for user: BOSS1
Statement failed, SQLSTATE = HY000
record not found for user: BOSS2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, user_boss1: User, user_boss2: User):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

