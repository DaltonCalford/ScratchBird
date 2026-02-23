#coding:utf-8

"""
ID:          issue-8485
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8485
TITLE:       Segfault/AV on incorrect databases.conf starting with subconfig (without line alias=database_path)
DESCRIPTION:
    Test makes temporary copy of $FB_HOME/databases.conf (assuming that QA account has access to this folder in order to overwrite content of this file).
    Then we save invalid content in the databases.conf and try to establish connection to test DB using isql.
    Before fix isql crashed, after fix "SQLSTATE = XX000 / databases.conf: illegal line" must be issued (twise).
NOTES:
    [03.04.2025] pzotov
    ### NOTE ### QA must run with access rights to $FB_HOME folder because databases.conf will be temporary overwritten.

    Confirmed bug (isql crashes): on 6.0.0.693; 5.0.3.1633; 4.0.6.3192; 3.0.13.33804
    Checked on 6.0.0.710; 5.0.3.1639; 4.0.6.3194; 3.0.13.33806
"""
from pathlib import Path
import shutil
import time
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')
tmp_file = temp_file('databases-conf.copy')

WRONG_DBCONF = """
#security.db = $(dir_secDb)/security3.fdb
{
  RemoteAccess = false
  DefaultDbCachePages = 320
  LockMemSize = 2M
}
"""

@pytest.mark.skip("INVESTIGATION NEEDED. Weird affecting on functional/syspriv/test_user_management_in_selfsec_db.py")
@pytest.mark.version('>=3.0.13')
def test_1(act: Action, tmp_file: Path, store_config: ConfigManager, capsys):

    store_config.replace('databases.conf', WRONG_DBCONF)
    try:
        act.isql(switches = ['-q'], input = f'connect {act.db.dsn};', combine_output = True)
    except Error as e:
        # Despite crash, no messages were issued here before fix.
        print(e)
    
    for line in act.stdout.splitlines():
        if (pos := line.lower().find('databases.conf')) > 0:
            print(line.lower()[pos:])
        else:
            print(line)

    act.expected_stdout = f"""
        Statement failed, SQLSTATE = XX000
        databases.conf: illegal line <master parameter is missing before subconfig start '{{'>
        Statement failed, SQLSTATE = XX000
        databases.conf: illegal line <master parameter is missing before subconfig start '{{'>
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
