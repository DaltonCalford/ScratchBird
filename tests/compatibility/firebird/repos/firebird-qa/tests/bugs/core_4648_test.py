#coding:utf-8

"""
ID:          issue-4962
ISSUE:       4962
TITLE:       no permission for CREATE access to DATABASE (for RDB$ADMIN)
DESCRIPTION:
JIRA:        CORE-4648
FBTEST:      bugs.core_4648
"""

import pytest
from io import BytesIO
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

db = db_factory()

act = python_act('db')

expected_stdout = """
    Starting backup...
    Backup finished.
    Starting restore using NON sysdba user account...
    Restore using NON sysdba user account finished.
    Starting ISQL using NON sysdba user account...
    Who am I:                       TMP$C4648
    Used protocol:                  TCP
    Connected to restored DB ?      YES
    Owner of DB is:                 TMP$C4648
    ISQL using NON sysdba user account finished.
"""

tmp_user = user_factory('db', name='tmp$c4648', password='123')
temp_db = temp_file('tmp4648.fdb')

script = """
set list on;
select
     a.mon$user as "Who am I:"
    ,left(a.mon$remote_protocol,3) as "Used protocol:"
    ,iif(m.mon$database_name containing 'tmp4648.fdb','YES','NO! ' || m.mon$database_name ) as "Connected to restored DB ?"
    ,m.mon$owner as "Owner of DB is:"
from mon$attachments a, mon$database m
where a.mon$attachment_id=current_connection;
commit;
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User, temp_db: Path, capsys):
    with act.db.connect() as con:
        c = con.cursor()
        #-- 'create ... grant admin role' or 'grant rdb$admin' are NOT neccessary
        #-- for enabling to creating database by non-sysdba user:
        c.execute('grant create database to user tmp$c4648')
        con.commit()
    #
    print ('Starting backup...')
    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        print ('Backup finished.')
    backup.seek(0)
    with act.connect_server(user=tmp_user.name, password=tmp_user.password) as srv:
        print ('Starting restore using NON sysdba user account...')
        srv.database.local_restore(database=temp_db, backup_stream=backup,
                                   flags=SrvRestoreFlag.REPLACE)
        print ('Restore using NON sysdba user account finished.')
    #
    print ('Starting ISQL using NON sysdba user account...')
    act.isql(switches=['-q', '-user', 'tmp$c4648', '-pas', '123', act.get_dsn(temp_db)],
               connect_db=False, input=script, credentials=False)
    print(act.stdout)
    print ('ISQL using NON sysdba user account finished.')
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
