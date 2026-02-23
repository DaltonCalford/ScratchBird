#coding:utf-8

"""
ID:          issue-6383
ISSUE:       6383
TITLE:       Win_Sspi in the list of auth plugins leads message about failed login to be
  changed (from 'Your user name and password are not defined...' to 'Missing security context ...')
DESCRIPTION:
  Test assumes that firebird.conf parameter AuthClient is: Legacy_Auth,Srp,Win_Sspi.
  This is done automaticaally by script that is launched daily on Firebird test machine.
JIRA:        CORE-6134
FBTEST:      bugs.core_6134
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
     set heading off;
     set term ^;
     execute block returns (whoami varchar(32)) as
     begin
         execute statement 'select current_user from rdb$database'
             as user 'SYSDBA'
             password 'ful1yWr0ng'  -- or use here some other password that is for sure invalid
         into whoami;
         suspend;
     end
     ^
"""

act = isql_act('db', test_script, substitutions=[('-At block line: [\\d]+, col: [\\d]+', '')])

expected_stderr = """
    Statement failed, SQLSTATE = 28000
    Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
"""

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
