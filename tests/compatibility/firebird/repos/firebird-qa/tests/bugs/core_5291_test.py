#coding:utf-8

"""
ID:          issue-5569
ISSUE:       5569
TITLE:       Error messages differ when regular user tries to RESTORE database, depending on his default role and (perhaps) system privilege USE_GBAK_UTILITY
DESCRIPTION:
JIRA:        CORE-5291
FBTEST:      bugs.core_5291
NOTES:
    [20.09.2022] pzotov
    One need to take in account that gbak produces error message with specifying PORT number after localhost,
    i.e.: "failed to create database localhost/NNNNN:<dbname>" (perhaps, only for non-default ports?).
    Checked on 5.0.0.591, 4.0.1.2692.
"""

import pytest
from pathlib import Path
from firebird.qa import *

substitutions = [ ('gbak: ERROR:([ \t])*no permission for CREATE access to DATABASE.*', 'gbak: ERROR:no permission for CREATE access to DATABASE'),
                  ('gbak: ERROR:([ \t])*failed to create database .*',                  'gbak: ERROR:failed to create database')
                ]

db = db_factory()

tmp_user_1 = user_factory('db', name='tmp$c5291_1', password='123')
tmp_user_2 = user_factory('db', name='tmp$c5291_2', password='456')
tmp_role = role_factory('db', name='role_for_use_gbak_utility')

act = python_act('db', substitutions=substitutions)

fbk_file = temp_file('tmp_core_5291.fbk')
fdb_file_1 = temp_file('tmp_core_5291_1.fdb')
fdb_file_2 = temp_file('tmp_core_5291_2.fdb')

expected_stdout = """
    Restore using SERVICES, user has NO default role:
    gbak: ERROR:no permission for CREATE access to DATABASE
    gbak: ERROR:failed to create database
    gbak: ERROR:    Exiting before completion due to errors
    gbak:Exiting before completion due to errors
    Restore using GBAK, user has NO default role:
    gbak: ERROR:no permission for CREATE access to DATABASE
    gbak: ERROR:failed to create database
    gbak:Exiting before completion due to errors
    Restore using SERVICES, user HAS default role with system privileges:
    gbak: ERROR:no permission for CREATE access to DATABASE
    gbak: ERROR:failed to create database
    gbak: ERROR:    Exiting before completion due to errors
    gbak:Exiting before completion due to errors
    Restore using GBAK, user HAS default role with system privileges:
    gbak: ERROR:no permission for CREATE access to DATABASE
    gbak: ERROR:failed to create database
    gbak:Exiting before completion due to errors
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user_1: User, tmp_user_2: User, fbk_file: Path, fdb_file_1: Path, fdb_file_2: Path, tmp_role: Role, capsys):
    with act.db.connect() as con:
        con.execute_immediate( f'alter role {tmp_role.name} set system privileges to USE_GBAK_UTILITY, SELECT_ANY_OBJECT_IN_DATABASE' )
        con.execute_immediate( f'grant default {tmp_role.name} to user {tmp_user_2.name}' )
    #
    act.gbak(switches=['-b', act.db.dsn, str(fbk_file)])

    #--------------------------------------------------------------------------
    # User 1
    act.reset()
    print('Restore using SERVICES, user has NO default role:')
    act.gbak(switches=[ '-se', 'localhost:service_mgr'
                        ,'-rep', str(fbk_file), str(fdb_file_1)
                        ,'-user', tmp_user_1.name
                        ,'-pas', tmp_user_1.password
                      ]
            ,credentials=False, combine_output = True)
    print(act.stdout)
    #--------------------------------------------------------------------------

    # User 1
    act.reset()
    print('Restore using GBAK, user has NO default role:')
    act.gbak(switches=[ '-rep', str(fbk_file), act.get_dsn(fdb_file_2)
                        ,'-user', tmp_user_1.name, '-pas', tmp_user_1.password
                      ]
             ,credentials=False, combine_output = True)
    print(act.stdout)
    #--------------------------------------------------------------------------

    # User 2
    act.reset()
    print('Restore using SERVICES, user HAS default role with system privileges:')
    act.gbak(switches=[ '-se', f'{act.host}:service_mgr'
                        ,'-rep', str(fbk_file), str(fdb_file_1)
                        ,'-user', tmp_user_2.name
                        ,'-pas', tmp_user_2.password
                      ]
             ,credentials=False, combine_output = True)
    print(act.stdout)
    #--------------------------------------------------------------------------

    # User 2
    act.reset()
    print('Restore using GBAK, user HAS default role with system privileges:')
    act.gbak(switches=[ '-rep', str(fbk_file), act.get_dsn(fdb_file_2),
                        '-user', tmp_user_2.name
                        ,'-pas', tmp_user_2.password
                      ]
             ,credentials=False, combine_output = True)
    print(act.stdout)
    #
    act.reset()

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
