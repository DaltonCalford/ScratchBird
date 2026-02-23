#coding:utf-8

"""
ID:          issue-8462
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8462
TITLE:       A user with "GRANT_REVOKE_ON_ANY_OBJECT" privilege can't revoke a role from himself if he is not a grantor
DESCRIPTION:
NOTES:
    [07.03.2025] pzotov
    Confirmed issue on 6.0.0.658.
    Checked on 6.0.0.660-6cbd3aa; 5.0.3.1630-81e0de9; 4.0.6.3190 -- all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name = 'tmp$8462', password = '123')
tmp_role = role_factory('db', name = 'role_8462')

act = python_act('db')

@pytest.mark.version('>=4.0.6')
def test_1(act: Action, tmp_user: User, tmp_role: Role):

    test_sql = f"""
        set list on;
        set term ^;
        execute block as
        begin
            execute statement 'drop role {tmp_role.name}';
        when any do
            begin
                --- nop ---
            end
        end
        ^
        set term ;^
        commit;

        grant RDB$ADMIN to {tmp_user.name};

        commit;
        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}' role rdb$admin;

        create role {tmp_role.name};
        grant {tmp_role.name} to {act.db.user};

        commit;
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        set count on;

        --select * from rdb$user_privileges p where p.rdb$relation_name = upper('{tmp_role.name}');
        revoke {tmp_role.name} from {act.db.user};
        commit;
        select * from rdb$user_privileges p where p.rdb$relation_name = upper('{tmp_role.name}');
    """

    expected_stdout = """
        Records affected: 0 
    """
    act.expected_stdout = expected_stdout
    act.isql(input = test_sql, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout

