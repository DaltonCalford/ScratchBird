#coding:utf-8

"""
ID:          issue-5022
ISSUE:       5022
TITLE:       Restore of shadowed database fails using -k ("restore without shadow") switch
DESCRIPTION:
NOTES:
[23.11.2021]
  For unknow reason, on v4.0.0.2496 the gstat -h does not report active shadow
  and test thus fail.
JIRA:        CORE-4715
FBTEST:      bugs.core_4715
"""

import pytest
from pathlib import Path
from firebird.qa import *

substitutions = [('^((?!HASH_IN_SOURCE|RDB\\$SHADOW_NUMBER|HASH_IN_RESTORED).)*$', ''),
                 ('.*Shadow 1:.*', 'Shadow present')]

init_script = """
    -- Confirmed on WI-T3.0.0.31374:
    -- command "gbak -rep -k c4715.fbk -user SYSDBA -pas masterke localhost/3000:<path>\\c4715-new.FDB"
    -- produces:
    -- gbak: ERROR:DELETE operation is not allowed for system table RDB$FILES
    -- gbak:Exiting before completion due to errors
    recreate table test(s varchar(30));
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=substitutions)

expected_stdout = """
HASH_IN_SOURCE                  1499836372373901520
RDB$SHADOW_NUMBER               1
Shadow present
HASH_IN_RESTORED                1499836372373901520
"""

bkp_file = temp_file('core_4715-shadowed.fbk')
fdb_file = temp_file('core_4715-restored.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, bkp_file: Path, fdb_file: Path, capsys):
    act.isql(switches=['-q'],
               input=f'''create shadow 1 '{act.db.db_path.with_suffix('.shd')}'; commit; insert into test select 'line #'||lpad(row_number()over(), 3, '0') from rdb$types rows 200; commit; set list on; select hash(list(s)) hash_in_source from test; select * from rdb$files;''')
    print(act.stdout)
    act.reset()
    act.isql(switches=[], input='show database;')
    print(act.stdout)
    act.reset()
    act.gbak(switches=['-b', act.db.dsn, str(bkp_file)])
    act.reset()
    act.gbak(switches=['-rep', '-k', str(bkp_file), str(fdb_file)])
    act.reset()
    act.isql(switches=['-q', str(fdb_file)], connect_db=False,
               input='set list on; select hash(list(s)) hash_in_restored from test;')
    print(act.stdout)
    #
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
