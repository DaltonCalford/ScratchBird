#coding:utf-8

"""
ID:          create-database-02
TITLE:       Create database: non sysdba user
DESCRIPTION:
FBTEST:      functional.database.create.02
NOTES:
    [13.01.2025] pzotov
    Added (temporary ?) 'credentials = False' to prevent ISQL from using '-USER ... -PASS ...'.
    This is needed since 6.0.0.570, otherwise we get (on attempting to create DB):
        Statement failed, SQLSTATE = 28000
        Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
        -Different logins in connect and attach packets - client library error
    (IMO, this is bug; see https://github.com/FirebirdSQL/firebird/issues/8385)
"""

import locale
from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [('[ \t]+', ' ')])

tmp_user = user_factory('db', name='tmp$boss', password='123')
test_db = temp_file('tmp4test.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User, test_db: Path):

    test_script = f"""
        set wng off;
        set bail on;
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        revoke all on all from {tmp_user.name};
        commit;

        --  ::: NB ::: do NOT miss specification of 'USER' or 'ROLE' clause in
        --  GRANT | REVOKE CREATE DATABASE, between `to` and login! Otherwise:
        --    Statement failed, SQLSTATE = 0A000
        --    unsuccessful metadata update
        --    -GRANT failed
        --    -feature is not supported
        --    -Only grants to USER or ROLE are supported for CREATE DATABASE
        grant create database to USER {tmp_user.name};
        --                       ^^^^
        grant drop database to USER {tmp_user.name};
        --                     ^^^^
        commit;
        create database 'localhost:{test_db}' user {tmp_user.name} password '{tmp_user.password}';

        set list on;
        select
             a.mon$user "Who am I ?"
            ,iif( m.mon$database_name containing '{test_db}' , 'YES', 'NO! ' || m.mon$database_name) "Am I on just created DB ?"
        from mon$database m cross join mon$attachments a
        where a.mon$attachment_id = current_connection;
        commit;
        drop database;
        set echo on;
        -- Done.
    """

    act.expected_stdout = f"""
        Who am I ?                {tmp_user.name.upper()}
        Am I on just created DB ? YES
        -- Done.
    """
    act.isql(switches=['-q'], input=test_script, connect_db=False, combine_output = True, credentials = False, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
