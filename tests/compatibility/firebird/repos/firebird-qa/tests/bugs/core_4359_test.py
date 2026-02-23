#coding:utf-8

"""
ID:          issue-4681
ISSUE:       4681
TITLE:       non-priviledged user can insert and update rdb$database
DESCRIPTION:
JIRA:        CORE-4359
FBTEST:      bugs.core_4359
NOTES:
    [29.06.2025] pzotov
    Re-implemented: use f-notation to substitute fixture values in the expected output.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='boss', password='123')

act = isql_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):

    test_script = f"""
        -- Test scenario attempts to modify (or lock record) from RDB$DATABASE
        -- both for SYSDBA and non-privileged user.
        set count on;

        insert into rdb$database(rdb$security_class) values('');
        delete from rdb$database where rdb$security_class = '';
        update rdb$database set rdb$security_class = rdb$security_class where rdb$security_class = '';
        select current_user from rdb$database with lock;

        commit;

        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';

        insert into rdb$database(rdb$security_class) values('');
        delete from rdb$database where rdb$security_class = '';
        update rdb$database set rdb$security_class = rdb$security_class where rdb$security_class = '';
        select current_user from rdb$database with lock;

        commit;
    """


    expected_stdout_3x = """
        Statement failed, SQLSTATE = 42000
        INSERT operation is not allowed for system table RDB$DATABASE

        Records affected: 0
        Records affected: 0
        Records affected: 0

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$DATABASE for update WITH LOCK

        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$DATABASE

        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$DATABASE

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$DATABASE

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$DATABASE for update WITH LOCK
    """

    expected_stdout_4x = f"""
        Statement failed, SQLSTATE = 42000
        INSERT operation is not allowed for system table RDB$DATABASE

        Records affected: 0
        Records affected: 0
        Records affected: 0

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$DATABASE for update WITH LOCK

        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$DATABASE
        -Effective user is {tmp_user.name.upper()}

        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$DATABASE
        -Effective user is {tmp_user.name.upper()}

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$DATABASE
        -Effective user is {tmp_user.name.upper()}

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$DATABASE for update WITH LOCK
    """

    expected_stdout_6x = f"""
        Statement failed, SQLSTATE = 42000
        INSERT operation is not allowed for system table "SYSTEM"."RDB$DATABASE"

        Records affected: 0
        Records affected: 0
        Records affected: 0

        Statement failed, SQLSTATE = HY000
        Cannot select system table "SYSTEM"."RDB$DATABASE" for update WITH LOCK

        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE "SYSTEM"."RDB$DATABASE"
        -Effective user is {tmp_user.name.upper()}

        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE "SYSTEM"."RDB$DATABASE"

        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE "SYSTEM"."RDB$DATABASE"
        -Effective user is {tmp_user.name.upper()}

        Statement failed, SQLSTATE = HY000
        Cannot select system table "SYSTEM"."RDB$DATABASE" for update WITH LOCK
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_4x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

