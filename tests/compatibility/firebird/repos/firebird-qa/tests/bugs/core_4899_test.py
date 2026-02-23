#coding:utf-8

"""
ID:          issue-5192
ISSUE:       5192
TITLE:       GFIX -online: message "IProvider::attachDatabase failed when loading mapping cache"
  appears in Classic (only) if access uses remote protocol
DESCRIPTION:
JIRA:        CORE-4899
FBTEST:      bugs.core_4899
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^((?!Attributes).)*$', ''), ('[\t]+', ' ')])

expected_stdout = """
Attributes full shutdown
Attributes
Attributes full shutdown
Attributes
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    # Trying to move database to OFFLINE:
    act.gfix(switches=['-shut', 'full', '-force', '0', str(act.db.db_path)])
    print(act.stdout)
    act.reset()
    act.gstat(switches=['-h', str(act.db.db_path)], connect_db=False)
    print(act.stdout)
    # Trying to move database online using LOCAL protocol:
    act.reset()
    act.gfix(switches=['-online', str(act.db.db_path)])
    print(act.stdout)
    # gfix attachment via local protocol reflects with following lines in trace:
    # 2015-08-24T18:30:03.2580 (2516:012417E0) ATTACH_DATABASE
    #	C:\\MIX\\FIREBIRD\\QA\\FBT-REPO\\TMP\\CORE4899-TMP.FDB (ATT_9, SYSDBA:NONE, NONE, <internal>)
    act.reset()
    act.gstat(switches=['-h', str(act.db.db_path)], connect_db=False)
    print(act.stdout)
    # --------------------- II ---------------
    act.reset()
    act.gfix(switches=['-shut', 'full', '-force', '0', str(act.db.db_path)])
    print(act.stdout)
    act.reset()
    act.gstat(switches=['-h', str(act.db.db_path)], connect_db=False)
    print(act.stdout)
    # Trying to move database online using REMOTE protocol:
    act.reset()
    act.gfix(switches=['-online', act.db.dsn])
    print(act.stdout)
    # Note: gfix attachment via remote protocol refects with following lines in trace:
    # 2015-08-24T18:30:03.8520 (3256:01B526A8) ATTACH_DATABASE
    #	C:\\MIX\\FIREBIRD\\QA\\FBT-REPO\\TMP\\CORE4899-TMP.FDB (ATT_9, SYSDBA:NONE, NONE, TCPv4:127.0.0.1)
    #	C:\\MIX\\firebird\\db30\\gfix.exe:1448
    act.reset()
    act.gstat(switches=['-h', str(act.db.db_path)], connect_db=False)
    print(act.stdout)
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
