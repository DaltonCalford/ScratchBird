#coding:utf-8

"""
ID:          issue-6250
ISSUE:       6250
TITLE:       gbak issues "Your user name and password are not defined" when command switch
  "-fe(tch_password) ..." is specified when run as service
DESCRIPTION:
  Test creates two files, one with correct SYSDBA password and second with invalid (hope that such password: T0t@1lywr0ng - is not in use for SYSDBA).
  Also, test exports default SYSDBA password ('masterkey' ) to ISC_PASSWORD variable.
  Then we do following:
  1) "gbak -fe <invalid_password_file>" - this should FAIL with issuing "user name and password are not defined" in STDERR,
    despite that ISC_USER is not empty and contains valid password
  2) UNSET variable ISC_PASSWORD and run "gbak -fe <correct_password_file>" - this should PASS without any STDOUT or STDERR.
JIRA:        CORE-6000
FBTEST:      bugs.core_6000
"""

import pytest
import os
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stderr = """
    gbak: ERROR:Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
    gbak:Exiting before completion due to errors
"""

pwd_file = temp_file('pwd.dat')
fbk_file = temp_file('tmp_core_6000.fbk')

@pytest.mark.version('>=3.0.5')
def test_1(act: Action, pwd_file: Path, fbk_file: Path):
    pwd_file.write_text('T0t@1lywr0ng')
    with act.envar('ISC_PASSWORD', act.db.password):
        act.expected_stderr = expected_stderr
        act.gbak(switches=['-b', '-se', 'localhost:service_mgr', '-user', act.db.user,
                           '-fe', str(pwd_file), act.db.dsn, str(fbk_file)], credentials=False)
        assert act.clean_stderr == act.clean_expected_stderr
    pwd_file.write_text(act.db.password)
    act.gbak(switches=['-b', '-se', 'localhost:service_mgr', '-user', act.db.user,
                       '-fe', str(pwd_file), act.db.dsn, str(fbk_file)], credentials=False)
