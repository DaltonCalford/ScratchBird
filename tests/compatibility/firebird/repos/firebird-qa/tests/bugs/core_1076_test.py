#coding:utf-8

"""
ID:          issue-1498
ISSUE:       1498
TITLE:       Services Manager and gsec truncate First.Middle.Last Name fields to 17 chars instead of 32 chars available in field definition
DESCRIPTION:
NOTES:
[11.01.2016]
  refactored for 3.0: use FBSVCMGR instead of GSEC. This was agreed with Alex, see his reply 11.01.2015 17:57.
JIRA:        CORE-1076
FBTEST:      bugs.core_1076
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

tmp_user = user_factory('db', name="Nebuchadnezzar2_King_of_Babylon",
                        password="Nebu_King_of_Babylon")

expected_stdout = """
SEC$USER_NAME                   NEBUCHADNEZZAR2_KING_OF_BABYLON
SEC$FIRST_NAME                  Nebuchadnezzar3_King_of_Babylon
SEC$MIDDLE_NAME                 Nebuchadnezzar4_King_of_Babylon
SEC$LAST_NAME                   Nebuchadnezzar5_King_of_Babylon
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
   with act.connect_server() as srv:
      sec_db = srv.info.security_database
   act.svcmgr(switches=['action_modify_user', 'dbname', sec_db,
                          'sec_username', tmp_user.name,
                          'sec_firstname', 'Nebuchadnezzar3_King_of_Babylon',
                          'sec_middlename', 'Nebuchadnezzar4_King_of_Babylon',
                          'sec_lastname', 'Nebuchadnezzar5_King_of_Babylon'])
   #
   act.script = f"""set list on;
select sec$user_name, sec$first_name, sec$middle_name, sec$last_name from sec$users
where upper(sec$user_name) = upper('{tmp_user.name}');
"""
   act.expected_stdout = expected_stdout
   act.execute()
   assert act.clean_stdout == act.clean_expected_stdout

