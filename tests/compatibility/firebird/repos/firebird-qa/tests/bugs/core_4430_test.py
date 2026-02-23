#coding:utf-8

"""
ID:          issue-4750
ISSUE:       4750
TITLE:       Properties of user created in Legacy_UserManager padded with space up to 10 character
DESCRIPTION:
JIRA:        CORE-4430
FBTEST:      bugs.core_4430
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp$c4430', password='123', first_name='john',
                        last_name='smith', plugin='Legacy_UserManager')

test_script = """
    -- Confirmed padded output in WI-T3.0.0.30809 Firebird 3.0 Alpha 2:
    --    SEC$USER_NAME                   TMP$C4430
    --    FIRST_NAME_WITH_DOT             john                            .
    --    LAST_NAME_WITH_DOT              smith                           .

    set list on;
    select
        sec$user_name,
        sec$first_name || '.' first_name_with_dot,
        sec$last_name  || '.' last_name_with_dot
    from sec$users
    where sec$user_name = upper('tmp$c4430');
"""

act = isql_act('db', test_script)

expected_stdout = """
    SEC$USER_NAME                   TMP$C4430
    FIRST_NAME_WITH_DOT             john.
    LAST_NAME_WITH_DOT              smith.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

