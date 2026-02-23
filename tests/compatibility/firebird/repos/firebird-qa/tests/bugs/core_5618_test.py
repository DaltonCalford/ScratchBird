#coding:utf-8

"""
ID:          issue-5884
ISSUE:       5884
TITLE:       Part of the pages of the second level blobs is not released when deleting relations
DESCRIPTION:
  We create table with blob field and write into it binary data with length that
  is too big to store such blob as level-0 and level-1. Filling is implemented as
  specified in:
    http://pythonhosted.org/fdb/differences-from-kdb.html#stream-blobs
  Then we drop table and close connection.
  Finally, we obtain firebird.log, run full validation (like 'gfix -v -full' does) and get firebird.log again.
  Comparison of two firebird.log versions should give only one difference related to warnings, and they count
  must be equal to 0.
JIRA:        CORE-5618
FBTEST:      bugs.core_5618
"""

import pytest
import zipfile
from difflib import unified_diff
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRepairFlag

init_script = """
      recreate table test(b blob sub_type 0);
"""

db = db_factory(init=init_script)

act = python_act('db')

blob_src = temp_file('core_5618.bin')

@pytest.mark.version('>=3.0.3')
def test_1(act: Action, blob_src: Path):
    zipped_blob_file = zipfile.Path(act.files_dir / 'core_5618.zip', at='core_5618.bin')
    blob_src.write_bytes(zipped_blob_file.read_bytes())
    #
    with act.db.connect() as con:
        c = con.cursor()
        with open(blob_src, mode='rb') as blob_handle:
            c.execute('insert into test (b) values (?)', [blob_handle])
        c.close()
        con.execute_immediate('drop table test')
        con.commit()
    #
    log_before = act.get_firebird_log()
    # Run full validation (this is what 'gfix -v -full' does)
    with act.connect_server() as srv:
        srv.database.repair(database=act.db.db_path,
                            flags=SrvRepairFlag.FULL | SrvRepairFlag.VALIDATE_DB)
        assert srv.readlines() == []
    #
    log_after = act.get_firebird_log()
    log_diff = [line.strip().upper() for line in unified_diff(log_before, log_after)
                if line.startswith('+') and 'WARNING' in line.upper()]
    assert log_diff == ['+\tVALIDATION FINISHED: 0 ERRORS, 0 WARNINGS, 0 FIXED']
