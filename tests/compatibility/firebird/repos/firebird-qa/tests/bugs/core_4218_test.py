#coding:utf-8

"""
ID:          issue-4543
ISSUE:       4543
TITLE:       Add database owner to mon$database
DESCRIPTION:
JIRA:        CORE-4218
NOTES:
    [13.01.2025] pzotov
    Added (temporary ?) 'credentials = False' to prevent ISQL from using '-USER ... -PASS ...'.
    This is needed since 6.0.0.570, otherwise we get (on attempting to create DB):
        Statement failed, SQLSTATE = 28000
        Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
        -Different logins in connect and attach packets - client library error
    (IMO, this is bug; see https://github.com/FirebirdSQL/firebird/issues/8385)
"""
import time
import locale
import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

tmp_user: User = user_factory('db', name='TMP_U4218', password='123')

act = python_act('db', substitutions = [('[ \t]+', ' ')])

test_db = temp_file('owner-db.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User, test_db: Path):
    with act.db.connect() as con:
        c = con.cursor()
        c.execute(f'grant create database to user {tmp_user.name}')
        con.commit()

    test_script = f"""
        -- set echo on;
        create database 'localhost:{test_db}' user {tmp_user.name} password '{tmp_user.password}';
        set list on;
        select current_user as who_am_i, mon$owner as who_is_owner from mon$database;
        commit;
        connect 'localhost:{test_db}' user {act.db.user} password '{act.db.password}';
        select current_user as who_am_i, mon$owner as who_is_owner from mon$database;
        commit;
        drop database;
        quit;
    """

    expected_stdout = f"""
        WHO_AM_I                        {tmp_user.name.upper()}
        WHO_IS_OWNER                    {tmp_user.name.upper()}
        WHO_AM_I                        {act.db.user}
        WHO_IS_OWNER                    {tmp_user.name.upper()}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input=test_script, connect_db=False, combine_output = True, credentials = False, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
