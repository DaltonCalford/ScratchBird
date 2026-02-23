#coding:utf-8

"""
ID:          issue-5118
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5118
TITLE:       grant create database to ROLE doesn`t work: "no permission for CREATE access to DATABASE ..." [CORE4821]
DESCRIPTION:
    In order to avoid remote connection to default security.db, we use pre-defined alias in the databases.conf,
    which points to SELF-SECURITY database that must be created in "$(dir_sampleDb)/qa/" directory. This folder
    is cleaned up before every test run, so we can be sure that it will not contain such database remaining from
    previous test session.
    Such database can be created using local protocol only, but we can further to create role IN THIS database
    and, most important, can specify this database as SECURITY for another DB which is to be created for check
    purposes of this test.
    First of specified databases has alias which is defined here as variable REQUIRED_ALIAS.
    Second database is referenced here via variable DB_BIND_ALIAS. This DB can be created only after REQUIRED_ALIAS.
    
    Test uses SQL which creates <REQUIRED_ALIAS> and user 'dba_4821' in it.
    Then SQL creates role 'db_maker', grants 'CREATE DATABASE' privilege to it, and further grants this role to
    user 'dba_4821'.
    Since this point, user 'dba_4821' must have ability to create another database, <REQUIRED_ALIAS>, by just
    issuing statement like 'CREATE DATABASE 'localhost:<DB_BIND_ALIAS>' user dba_4821 role db_maker'.
    Test verifies ability to do that.

    Finally, we drop database <DB_BIND_ALIAS> and, after returning from this SQL, change state of <REQUIRED_ALIAS>
    database to full shutdown. This allows us to drop this database file without getting error message like
    "SQLSTATE = 40001 / lock time-out on wait transaction / -object ... is in use" (caused by security nature of that
    database and fact that engine always attempts to keep security.db opened for at about 10 seconds).
JIRA:        CORE-4821
FBTEST:      bugs.core_4821
NOTES:
    [24.11.2021] pcisar
      Without change to databases.conf this test FAILs because it's not possible to grant create
      database to role tmp$db_creator although it exists (in test database):
      Statement failed, SQLSTATE = 28000
      unsuccessful metadata update
      -GRANT failed
      -SQL role TMP$DB_CREATOR does not exist
      -in security database

    [16.09.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    4. Test also used pre-defined alias with name defined by variable DB_BIND_ALIAS, with similar requirements
       to it as for REQUIRED_ALIAS. The only (and main) difference is that this alias has declaration of
       SecurityDatabase whith value = <REQUIRED_ALIAS>. So, this database is NOT self-security and also it does
       NOT use default security.db.

    Checked on Linux and Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730
"""
import locale
import re
import sys
from pathlib import Path
import time

import pytest
from firebird.qa import *

# Pre-defined alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB home folder, with replacing
# databases.conf there:
#
REQUIRED_ALIAS = 'tmp_core_4821_alias'

# Pre-defined alias of database which we want to create using remote protocol and thus
# have to set REQUIRED_ALIAS as SECURITY database (diff than default sec.db):
#
DB_BIND_ALIAS = 'tmp_bind_4821_alias'

db = db_factory()
db_clone = db_factory(filename = '#' + DB_BIND_ALIAS, do_not_create = True, do_not_drop = True)

tmp_user = user_factory('db', name='dba_4821', password='123', do_not_create = True)
tmp_role = role_factory('db', name='db_maker', do_not_create = True)

act = python_act('db')
act_clone = python_act('db_clone')

#----------------------------------------------------
def get_filename_by_alias(act: Action, alias_from_dbconf):
    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + alias_from_dbconf + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
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

    #------------------------------------------------------------------
    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    return tmp_fdb

#----------------------------------------------------

@pytest.mark.version('>=3.0.5')
def test_1(act: Action,
           act_clone: Action,
           tmp_user: User,
           tmp_role: Role,
           capsys
          ):

    db_work_file = get_filename_by_alias(act, REQUIRED_ALIAS)
    db_clone_file = get_filename_by_alias(act_clone, DB_BIND_ALIAS)

    #----------------------------------------------------

    dba_pswd = 'alterkey'

    # The role MUST be created in security database!
    #
    sql_test = f"""
        set wng off;
        --set echo on;
        set list on;
        create database '{REQUIRED_ALIAS}' user {tmp_user.name};
        
        create role {tmp_role.name};
        grant create database to role {tmp_role.name};
        grant drop database to role {tmp_role.name};

        create user {tmp_user.name} password '{tmp_user.password}'
        -- ### ACHTUNG ### DO NOT specify in FB 4.x and 5.x:
        -- REVOKE ADMIN ROLE; ==> weird "SQLSTATE = 42000 / add record error ; ... / Zero length identifiers are not allowed"
        ;
        commit;
        grant {tmp_role.name} to {tmp_user.name}; -- actually not requied in FB 4.x and 5.x!
        commit;


        -- NOTE: following statement requires ROLE specification only in FB 3.x,
        -- otherwise:
        --     Statement failed, SQLSTATE = 28000
        --     no permission for CREATE access to DATABASE tmp_bind_4821_alias
        -- ### NO ### such requiremtnt in FB 4.x+
        create database 'localhost:{DB_BIND_ALIAS}' user {tmp_user.name} password '{tmp_user.password}'
            role {tmp_role.name} -- actually not requied in FB 4.x and 5.x!
        ;
        select
             iif( upper(mon$database_name) = upper('{db_clone_file}'), 'EXPECTED', 'UNEXPECTED! ' || coalesce(mon$database_name,'[null]')) as created_db_name
            ,mon$owner as db_owner
            ,mon$sec_database
        from mon$database;
        rollback;
        drop database;
    """

    try:
        act.expected_stdout = f"""
            CREATED_DB_NAME                 EXPECTED
            DB_OWNER                        DBA_4821
            MON$SEC_DATABASE                Other
        """
        # act.expected_stdout = ''
        act.isql(switches=['-q'], input=sql_test, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    finally:
        for p in (db_work_file, db_clone_file):
            if Path.exists(p):
                # Change DB state to full shutdown in order to have ability to drop database file.
                # This is needed because when DB is self-security then it will be kept opened for 10s
                # (as it always occurs for default security<N>.fdb). Set linger to 0 does not help.
                # Attempt to use 'drop database' fails with:
                # "SQLSTATE = 40001 / lock time-out on wait transaction / -object ... is in use"
                act.gfix(switches=['-shut', 'full', '-force', '0', f'localhost:{p}', '-user', tmp_user.name, '-pas', tmp_user.password], io_enc = locale.getpreferredencoding(), credentials = False, combine_output = True)
                p.unlink()
                assert '' == capsys.readouterr().out
