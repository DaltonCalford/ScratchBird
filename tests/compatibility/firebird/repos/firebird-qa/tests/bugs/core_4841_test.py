#coding:utf-8

"""
ID:          issue-5137
ISSUE:       5137
TITLE:       Make message about missing password being always displayed as reply on attempt to issue CREATE new login without PASSWORD clause
DESCRIPTION:
JIRA:        CORE-4841
FBTEST:      bugs.core_4841
NOTES:
  [04.02.2022] pcisar
    Test fails with 3.0.8, because command
      create or alter user u01 tags (password = 'foo');
    does not produce any error.

  [18.09.2022] pzotov
  Could NOT find any problems on both Windows and Linux. Test works FINE.
  Checked on 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730 (SS/CS).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- All following statements must fail with message that contains phrase:
    -- "Password must be specified when creating user"
    create user u01;
    create user u01 using plugin Srp;
    create user u01 firstname 'john';
    create user u01 grant admin role;
    create user u01 inactive;
    create or alter user u01 tags (password = 'foo');
    create user password;
"""

act = isql_act('db', test_script, substitutions=[('[-]?Password', 'Password')])

expected_stderr = """
Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE USER U01 failed
-Password must be specified when creating user

Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE USER U01 failed
-Password must be specified when creating user

Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE USER U01 failed
-Password must be specified when creating user

Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE USER U01 failed
-Password must be specified when creating user

Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE USER U01 failed
-Password must be specified when creating user

Statement failed, SQLSTATE = HY000
Password must be specified when creating user

Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE USER PASSWORD failed
-Password must be specified when creating user
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
