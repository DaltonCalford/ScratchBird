#coding:utf-8

"""
ID:          issue-7178
ISSUE:       7178
TITLE:       DEFAULTed grants to PUBLIC must act as DEFAULTed to every user
NOTES:
    [21.02.2023] pzotov
    Confirmed problem on 5.0.0.494: role was not default when checked via RDB$ROLE_IN_USE()
    Checked on 5.0.0.958
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name = 'tmp_user_7178', password = '123')
tmp_role = role_factory('db', name = 'tmp_role_7178')
act = python_act('db')

@pytest.mark.version('>=5.0')
def test_1(act: Action, tmp_user: User, tmp_role: Role):

    test_script = f"""
        set list on;

        grant default {tmp_role.name} to public;
        commit;

        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';

        select current_user as who_am_i,cast(r.rdb$role_name as varchar(31)) as role_name, rdb$role_in_use(r.rdb$role_name) role_in_use
        from rdb$roles r
        where r.rdb$role_name = upper('{tmp_role.name}');
    """

    expected_stdout = f"""
        WHO_AM_I                        {tmp_user.name.upper()}
        ROLE_NAME                       {tmp_role.name.upper()}
        ROLE_IN_USE                     <true>
    """
    
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
