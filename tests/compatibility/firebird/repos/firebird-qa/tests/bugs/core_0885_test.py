#coding:utf-8

"""
ID:          issue-1278
ISSUE:       1278
TITLE:       It is impossible to take away rights on update of a column
DESCRIPTION:
JIRA:        CORE-885
FBTEST:      bugs.core_0885
NOTES:
    [23.06.2025] pzotov
    Reimplemented: removed usage of hard-coded values for user and role name.
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
import locale

import pytest
from firebird.qa import *

db = db_factory()

tmp_usr_senior = user_factory('db', name='tmp_0885_john_senior', password='sen')
tmp_usr_junior = user_factory('db', name='tmp_0885_mick_junior', password='jun')
tmp_role = role_factory('db', name='tmp_0885_modifier', do_not_create=True)

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' '), ('-Effective user is.*', '')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', substitutions = substitutions)

expected_stdout = """
    ID 1
    TEXT Initial data, added by SYSDBA
    CHANGED_BY_USER SYSDBA
    CHANGED_BY_ROLE NONE
    ID 2
    TEXT Initial data, added by SYSDBA
    CHANGED_BY_USER SYSDBA
    CHANGED_BY_ROLE NONE
    ID 3
    TEXT Initial data, added by SYSDBA
    CHANGED_BY_USER SYSDBA
    CHANGED_BY_ROLE NONE
    USER TMP_0885_MICK_JUNIOR
    ROLE TMP_0885_MODIFIER
    ID 1
    TEXT Update-1: through the ROLE
    CHANGED_BY_USER TMP_0885_MICK_JUNIOR
    CHANGED_BY_ROLE TMP_0885_MODIFIER
    ID 2
    TEXT Initial data, added by SYSDBA
    CHANGED_BY_USER SYSDBA
    CHANGED_BY_ROLE NONE
    ID 3
    TEXT Initial data, added by SYSDBA
    CHANGED_BY_USER SYSDBA
    CHANGED_BY_ROLE NONE
    USER TMP_0885_JOHN_SENIOR
    ROLE NONE
    ID 1
    TEXT Update-1: through the ROLE
    CHANGED_BY_USER TMP_0885_MICK_JUNIOR
    CHANGED_BY_ROLE TMP_0885_MODIFIER
    ID 2
    TEXT Update-2: directly by USER
    CHANGED_BY_USER TMP_0885_JOHN_SENIOR
    CHANGED_BY_ROLE NONE
    ID 3
    TEXT Initial data, added by SYSDBA
    CHANGED_BY_USER SYSDBA
    CHANGED_BY_ROLE NONE
    USER TMP_0885_MICK_JUNIOR
    ROLE TMP_0885_MODIFIER
    Statement failed, SQLSTATE = 28000
    no permission for UPDATE access to TABLE TEST
    ID 1
    TEXT Update-1: through the ROLE
    CHANGED_BY_USER TMP_0885_MICK_JUNIOR
    CHANGED_BY_ROLE TMP_0885_MODIFIER
    ID 2
    TEXT Update-2: directly by USER
    CHANGED_BY_USER TMP_0885_JOHN_SENIOR
    CHANGED_BY_ROLE NONE
    ID 3
    TEXT Initial data, added by SYSDBA
    CHANGED_BY_USER SYSDBA
    CHANGED_BY_ROLE NONE
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_usr_senior: User, tmp_usr_junior: User, tmp_role: Role):

    test_sql = f"""
        set wng off;
        set list on;

        recreate table test(id int, text varchar(30), changed_by_user varchar(31), changed_by_role varchar(31));
        commit;

        set term ^;
        create trigger test_biu for test active before insert or update position 0 as
        begin
            new.changed_by_user = current_user;
            new.changed_by_role = current_role;
        end
        ^
        set term ;^
        commit;

        insert into test(id, text) values(1, 'Initial data, added by SYSDBA');
        insert into test(id, text) values(2, 'Initial data, added by SYSDBA');
        insert into test(id, text) values(3, 'Initial data, added by SYSDBA');
        select * from test;
        commit;

        grant select on test to public;
        grant create role to {tmp_usr_senior.name};
        grant update(text) on test to {tmp_usr_senior.name} with grant option;
        commit;

        ----------------------------------------

        --set echo on;
        connect '{act.db.dsn}' user '{tmp_usr_senior.name}' password '{tmp_usr_senior.password}';
        create role {tmp_role.name};
        commit;
        grant update (text) on test to {tmp_role.name}; -- this CAN be done by john_senior because he was granted to control access on this field
        grant {tmp_role.name} to {tmp_usr_junior.name}; -- this CAN be done by john_senior because he CREATED role 'modifier' and thus he is MEMBER of it.
        commit;

        connect '{act.db.dsn}' user '{tmp_usr_junior.name}' password '{tmp_usr_junior.password}' role '{tmp_role.name}';
        select current_user, current_role from rdb$database;
        update test set text = 'Update-1: through the ROLE' where id = 1;
        select * from test;

        commit;

        connect '{act.db.dsn}' user '{tmp_usr_senior.name}' password '{tmp_usr_senior.password}';
        select current_user, current_role from rdb$database;
        update test set text = 'Update-2: directly by USER' where id = 2;
        select * from test;
        commit;

        connect '{act.db.dsn}' user '{tmp_usr_senior.name}' password '{tmp_usr_senior.password}';

        -- ###########################################################################################
        -- ###   H e r e    w e    R E V O K E    r i g h t   t o    u p d a t e     c o l u m n   ###
        -- ###########################################################################################
        -- ::: NB ::: See CORE-4836:
        -- As of WI-T3.0.0.31873, if we want to revoke privilege on certain COLUMN update, we must do
        -- it immediatelly after reconnect, NO issuing any DML here (like `select * from test` etc).

        revoke update(text) on test from {tmp_role.name}; -------------  ###########   R E V O K E   ########
        commit;

        connect '{act.db.dsn}' user '{tmp_usr_junior.name}' password '{tmp_usr_junior.password}' role '{tmp_role.name}';
        select current_user, current_role from rdb$database;
        update test set text = 'Update-3: again using ROLE' where id = 3;
        select * from test;

        commit;

        connect '{act.db.dsn}' user 'SYSDBA' password 'masterkey';

        --drop role {tmp_role.name};
        drop table test;
        commit;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_sql, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout

