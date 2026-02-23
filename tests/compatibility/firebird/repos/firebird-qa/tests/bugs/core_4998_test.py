#coding:utf-8

"""
ID:          issue-5286
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5286
TITLE:       Both client and server could not close connection after failed authentification
DESCRIPTION:
    [FBT only] Reproduced on 3.0.0.32136 RC1 with firebird.conf:
        AuthServer = Legacy_Auth,Srp
        AuthClient = Srp,Legacy_Auth

JIRA:        CORE-4998
FBTEST:      bugs.core_4998
NOTES:
    [18.08.2022] pzotov
    There is a problem with issue reproduction if we use firebird-driver for work with too old FB version!
    Connection is established OK but an attempt to start transaction (with 3.0.0.32136 RC1) fails:
        INTERNALERROR> firebird.driver.types.DatabaseError: invalid format for transaction parameter block
        INTERNALERROR> -wrong version of transaction parameter block
    Trace shows following errors  in that case:
        335544331 : invalid format for transaction parameter block
        335544411 : wrong version of transaction parameter block

    Test applies the same scenario as was described in the 1st message of this ticket, and checks that
    there is no difference in the content of firebird.log before and after failed attempts to connect.
    Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535
"""

import os
import locale
import re
from difflib import unified_diff
import time

import pytest
from firebird.qa import *

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass


db = db_factory()

substitutions = [('Your user name and password are not defined.*', '')]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    fblog_1 = act.get_firebird_log()

    sql_chk = f"""
        connect 'inet://{act.db.db_path}' user sysdba password 'inv@l1d_1';
        connect 'inet://{act.db.db_path}' user sysdba password 'inv@l1d_2';
        connect 'inet://{act.db.db_path}' user sysdba password '{act.db.password}';
        set heading off;
        select sign(current_connection) from rdb$database;
        quit;
    """

    act.expected_stdout = """
        Statement failed, SQLSTATE = 28000
        Statement failed, SQLSTATE = 28000
        1
    """
    act.isql(switches = ['-q'], input = sql_chk, connect_db = False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    time.sleep(1) # Allow content of firebird log be fully flushed on disk.
    fblog_2 = act.get_firebird_log()
   
    for line in unified_diff(fblog_1, fblog_2):
        if line.startswith('+'):
            print(line)

    expected_stdout_log_diff = ''
    act.expected_stdout = expected_stdout_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
