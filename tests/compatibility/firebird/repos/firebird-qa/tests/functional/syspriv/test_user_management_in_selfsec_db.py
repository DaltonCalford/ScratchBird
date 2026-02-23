#coding:utf-8

"""
ID:          user-management-in-selfsec-db
ISSUE:
TITLE:       Self-secutity DB: system privilege USER_MANAGEMENT must allow only to add/alter/drop user. No access to any user tables must be granted.
DESCRIPTION:
    NOTE: there is no difference between user who is granted with admin role when it was created and user who has no granted with this but
    has system privilege 'USER_MANAGEMENT': both of them can *only* add/edit/drop another users and no other actions.
    For example, they can give grants to just created users and can not select for any user-defined tables (until this was explicitly granted).

    But if we create user <U01> on SELF-SECURITY database and give to him admin role ('CREATE USER ... GRANT ADMIN ROLE') then this <U01> will
    be able to do such actions: he can grant rights to other users etc.
    In contrary to this, if we create user <U02> in such self-security DB but instead grant to him sytem privilege USER_MANAGEMENT then he
    will NOT be able to do these actions. Only create/alter/drop users will be avaliable to him.

    Test verifies exactly this case: abilities of user created inSELF-SECURITY database with granting to him privilege USER_MANAGEMENT.

    Discussed with Alex, letters since 12-aug-2021 21:12, subj:
    "system priv. USER_MANAGEMENT (Manage users): what ability is provided by this privilege that can not be gained by 'create user ... grant admin role' ?"

    Checked on 5.0.0.139 (SS/CS), 4.0.1.2568 (SS/CS).
FBTEST:      bugs.user_management
NOTES:
    [20.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS)
       and SecurityDatabase in its details which points to that alias, thus making such
       database be self-security. Database file for that alias must NOT exist in the
       QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Checked on 5.0.0.623, 4.0.1.2692 - both on Windows and Linux.
"""
import locale
import re
import time
from pathlib import Path
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

REQUIRED_ALIAS = 'tmp_syspriv_alias'
db = db_factory()

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_6147_alias = $(dir_sampleDb)/qa/tmp_core_6147.fdb
                # - then we extract filename: 'tmp_core_6147.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    tmp_dba_helper = 'tmp_supervisor'
    check_sql = f'''
        set list on;
        set wng off;
        set count on;
        set width mon$user 15;
        set width mon$role 15;
        set width sec$plugin 10;

        create database '{REQUIRED_ALIAS}' user {act.db.user};
        create user {act.db.user} password '{act.db.password}';
        create user {tmp_dba_helper} password '123';
        commit;

        recreate table test_ss(id int);
        commit;

        create or alter view v_check as
        select sec$user_name, sec$first_name, sec$admin,sec$active
        from sec$users where sec$user_name in (upper('stock_boss'), upper('stock_mngr'))
        ;
        grant select on v_check to public;
        commit;

        create role r_for_grant_revoke_any_ddl_right set system privileges to USER_MANAGEMENT;
        commit;
        grant default r_for_grant_revoke_any_ddl_right to user {tmp_dba_helper};
        commit;

        connect 'localhost:{REQUIRED_ALIAS}' user {tmp_dba_helper} password '123';

        select current_user as who_am_i,r.rdb$role_name,rdb$role_in_use(r.rdb$role_name),r.rdb$system_privileges,m.mon$sec_database
        from mon$database m cross join rdb$roles r
        ;
        commit;

        -- set echo on;
        -- Must PASS:
        create or alter user stock_boss password '123';
        alter user stock_boss firstname 'foo-rio-bar' password '456';
        create or alter user stock_mngr password '123';
        alter user stock_mngr inactive;
        commit;

        -- Must show 2 records (for users who have been just created):
        select * from v_check;

        -- must FAIL!
        grant select on test_ss to stock_mngr;
        commit;

        -- must FAIL!
        select * from test_ss;
        commit;

        -- Must PASS:
        drop user stock_boss;
        drop user stock_mngr;
        commit;

        -- Must show NO records (because users must be successfully dropped):
        select * from v_check;
        quit;
    '''

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    TEST_TABLE_NAME = 'TEST_SS' if act.is_version('<6') else  f'{SQL_SCHEMA_PREFIX}"TEST_SS"'
    act.expected_stdout = f"""
        WHO_AM_I                        {tmp_dba_helper.upper()}
        RDB$ROLE_NAME                   RDB$ADMIN
        RDB$ROLE_IN_USE                 <false>
        RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
        MON$SEC_DATABASE                Self

        WHO_AM_I                        {tmp_dba_helper.upper()}
        RDB$ROLE_NAME                   R_FOR_GRANT_REVOKE_ANY_DDL_RIGHT
        RDB$ROLE_IN_USE                 <true>
        RDB$SYSTEM_PRIVILEGES           0200000000000000
        MON$SEC_DATABASE                Self
        Records affected: 2

        SEC$USER_NAME                   STOCK_BOSS
        SEC$FIRST_NAME                  foo-rio-bar
        SEC$ADMIN                       <false>
        SEC$ACTIVE                      <true>

        SEC$USER_NAME                   STOCK_MNGR
        SEC$FIRST_NAME                  <null>
        SEC$ADMIN                       <false>
        SEC$ACTIVE                      <false>

        Records affected: 2

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -GRANT failed
        -no SELECT privilege with grant option on table/view {TEST_TABLE_NAME}

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TEST_TABLE_NAME}
        -Effective user is TMP_SUPERVISOR

        Records affected: 0
    """
    act.isql(switches = ['-q'], input = check_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    try:
        # Change DB state to full shutdown in order to have ability to drop database file.
        # This is needed because when DB is self-security then it will be kept opened for 10s
        # (as it always occurs for common security.db). Set linger to 0 does not help.
        act.gfix(switches=['-shut', 'full', '-force', '0', f'localhost:{REQUIRED_ALIAS}' ], io_enc = locale.getpreferredencoding(), combine_output = True)
    except DatabaseError as e:
        print(e.__str__())
        print(e.gds_codes)
    finally:
        assert act.stdout == '', f'Could not change test DB state to full shutdown, {act.return_code=}'
        act.reset()
        tmp_fdb.unlink()
