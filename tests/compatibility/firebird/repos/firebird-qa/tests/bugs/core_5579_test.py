#coding:utf-8

"""
ID:          issue-5846
ISSUE:       5846
TITLE:       request synchronization error in the GBAK utility (restore)
DESCRIPTION:
  Database for this test was created beforehand on 2.5.7 with intentionally broken not null constraint.
  It was done using direct RDB$ table modification:
  ---
   recreate table test(id int not null,fn int);
   insert into test(id, fn) values(1, null);
   insert into test(id, fn) values(2, null); -- 2nd record must also present!
   commit;
   -- add NOT NULL, direct modify rdb$ tables (it is allowed before 3.0):
   update rdb$relation_fields set rdb$null_flag = 1
   where rdb$field_name = upper('fn') and rdb$relation_name = upper('test');
   commit;
  ---
  We try to restore .fbk which was created from that DB on current FB snapshot and check that restore log
  does NOT contain phrase 'request synchronization' in any line.
JIRA:        CORE-5579
FBTEST:      bugs.core_5579
"""

import pytest
import re
import zipfile
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

db = db_factory()

act = python_act('db')

fbk_file = temp_file('core_5579_broken_nn.fbk')
fdb_file = temp_file('core_5579_broken_nn.fdb')

@pytest.mark.version('>=2.5.8')
def test_1(act: Action, fdb_file: Path, fbk_file: Path):
    pattern = re.compile('[.*]*request\\s+synchronization\\s+error\\.*', re.IGNORECASE)
    zipped_fbk_file = zipfile.Path(act.files_dir / 'core_5579_broken_nn.zip',
                                   at='core_5579_broken_nn.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())
    with act.connect_server() as srv:
        srv.database.restore(database=fdb_file, backup=fbk_file,
                             flags=SrvRestoreFlag.ONE_AT_A_TIME | SrvRestoreFlag.CREATE)

        # before this ticket was fixed restore fails with: request synchronization error
        for line in srv:
            if pattern.search(line):
                pytest.fail(f'RESTORE ERROR: {line}')
