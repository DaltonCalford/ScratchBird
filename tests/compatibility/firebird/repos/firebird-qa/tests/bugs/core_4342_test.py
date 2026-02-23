#coding:utf-8

"""
ID:          issue-4664
ISSUE:       4664
TITLE:       Non-privileged user can delete records from RDB$SECURITY_CLASSES table
DESCRIPTION:
JIRA:        CORE-4342
FBTEST:      bugs.core_4342
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
tmp_boss = user_factory('db', name='boss', password='123')
tmp_mngr = user_factory('db', name='mngr', password='456')

substitutions = [('[\t ]+', ' ')]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_boss: User, tmp_mngr: User):

    test_script = f"""
        -- Add these DDL privileges in order to have some rows in
        -- rdb$security_classes table for user {tmp_boss.name}:
        grant create table to {tmp_boss.name};
        grant alter any table to {tmp_boss.name};
        grant drop any table to {tmp_boss.name};
        commit;

        set list on;
        select current_user,count(*) acl_count from rdb$security_classes where rdb$acl containing '{tmp_boss.name}';

        select 1 from rdb$security_classes where rdb$acl containing '{tmp_boss.name}' with lock;
        update rdb$security_classes set rdb$security_class = rdb$security_class where rdb$acl containing '{tmp_boss.name}';
        delete from rdb$security_classes where rdb$acl containing '{tmp_boss.name}';
        commit;

        connect '{act.db.dsn}' user '{tmp_mngr.name}' password '{tmp_mngr.password}';
        select current_user,count(*) acl_count from rdb$security_classes where rdb$acl containing '{tmp_boss.name}';

        select 1 from rdb$security_classes where rdb$acl containing '{tmp_boss.name}' with lock;
        update rdb$security_classes set rdb$security_class = rdb$security_class where rdb$acl containing '{tmp_boss.name}';
        delete from rdb$security_classes where rdb$acl containing '{tmp_boss.name}';
        commit;
    """

    expected_stdout_3x = f"""
        USER                            {act.db.user.upper()}
        ACL_COUNT                       1

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$SECURITY_CLASSES for update WITH LOCK

        Statement failed, SQLSTATE = 42000
        UPDATE operation is not allowed for system table RDB$SECURITY_CLASSES

        Statement failed, SQLSTATE = 42000
        DELETE operation is not allowed for system table RDB$SECURITY_CLASSES

        USER                            {tmp_mngr.name.upper()}
        ACL_COUNT                       1

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$SECURITY_CLASSES for update WITH LOCK

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$SECURITY_CLASSES

        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$SECURITY_CLASSES
    """

    expected_stdout_5x = f"""
        USER                            {act.db.user.upper()}
        ACL_COUNT                       1

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$SECURITY_CLASSES for update WITH LOCK

        Statement failed, SQLSTATE = 42000
        UPDATE operation is not allowed for system table RDB$SECURITY_CLASSES

        Statement failed, SQLSTATE = 42000
        DELETE operation is not allowed for system table RDB$SECURITY_CLASSES

        USER                            {tmp_mngr.name.upper()}
        ACL_COUNT                       1

        Statement failed, SQLSTATE = HY000
        Cannot select system table RDB$SECURITY_CLASSES for update WITH LOCK

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$SECURITY_CLASSES
        -Effective user is {tmp_mngr.name.upper()}

        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$SECURITY_CLASSES
        -Effective user is {tmp_mngr.name.upper()}
    """


    expected_stdout_6x = f"""
        USER                            {act.db.user.upper()}
        ACL_COUNT                       1

        Statement failed, SQLSTATE = HY000
        Cannot select system table "SYSTEM"."RDB$SECURITY_CLASSES" for update WITH LOCK

        Statement failed, SQLSTATE = 42000
        UPDATE operation is not allowed for system table "SYSTEM"."RDB$SECURITY_CLASSES"
        Statement failed, SQLSTATE = 42000

        DELETE operation is not allowed for system table "SYSTEM"."RDB$SECURITY_CLASSES"
        USER                            {tmp_mngr.name.upper()}
        ACL_COUNT                       1

        Statement failed, SQLSTATE = HY000
        Cannot select system table "SYSTEM"."RDB$SECURITY_CLASSES" for update WITH LOCK

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE "SYSTEM"."RDB$SECURITY_CLASSES"
        -Effective user is {tmp_mngr.name.upper()}

        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE "SYSTEM"."RDB$SECURITY_CLASSES"
        -Effective user is {tmp_mngr.name.upper()}
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
