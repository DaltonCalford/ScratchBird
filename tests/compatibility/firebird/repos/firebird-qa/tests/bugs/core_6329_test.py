#coding:utf-8

"""
ID:          issue-6570
ISSUE:       6570
TITLE:       GBAK with service_mgr and WinSSPI authentication for Windows SYSTEM user producing error in clumplet API
DESCRIPTION: Initially checked on 4.0.0.2066 SS/CS, 3.0.6.33212 SS/CS.
JIRA:        CORE-6329
FBTEST:      bugs.core_6329
NOTES:
    [19.06.2022] pzotov
    Confirmed bug on: 3.0.6.33301, 4.0.0.2035
    Got: "gbak: ERROR:Internal error when using clumplet API: attempt to store <NNN> bytes in a clumplet with maximum size 255 bytes"
    NB: ISC_* variables must be removed from environtment for this test properly run.
    Checked on 4.0.1.2692, 3.0.8.33535.
"""
import os
import socket
import getpass
from pathlib import Path

import pytest
from firebird.qa import *

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

db = db_factory()
act = python_act('db')

tmp_fbk = temp_file( filename = 'tmp_core_6329.fbk')

@pytest.mark.version('>=3.0.6')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_fbk: Path, capsys):
    THIS_COMPUTER_NAME = socket.gethostname()
    CURRENT_WIN_USER = getpass.getuser()

    map_sql = f"""
        create or alter global mapping tmp_mapping_6329 using plugin win_sspi from user "{THIS_COMPUTER_NAME}\\{CURRENT_WIN_USER}" to user {act.db.user};
        commit;
    """

    act.expected_stdout = ''
    act.isql(switches=['-q'], input=map_sql, combine_output = True)
    act_retcode = act.return_code
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    if act_retcode == 0:
       gbak_output = ''
       try:
           act.gbak(switches=['-b', '-se', 'localhost:service_mgr', str(act.db.db_path), str(tmp_fbk)], credentials=False, combine_output = True)
           gbak_output = act.clean_stdout
       finally:
           act.isql(switches=['-q'], input='drop global mapping tmp_mapping_6329;', combine_output = True)
       
       assert gbak_output == ''

