#coding:utf-8

"""
ID:          issue-6115
ISSUE:       6115
TITLE:       Latest builds of Firebird 4.0 cannot backup DB with generators which contains space in the names
DESCRIPTION:
  Decided to apply test also against Firebird 3.x
NOTES:
  As of nowadays, it  is still possible to create sequence with name = single space character.
  See note in ticket, 26/Jun/18 07:58 AM.
JIRA:        CORE-5855
FBTEST:      bugs.core_5855
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_script = """
    create sequence "new sequence" start with 123 increment by -456;
    commit;
    comment on sequence "new sequence" is 'foo rio bar';
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('BLOB_ID.*', '')])

expected_stdout = """
    SEQ_NAME                        new sequence
    SEQ_INIT                        123
    SEQ_INCR                        -456
    foo rio bar
"""

test_script = """
    set list on;
    set blob all;
    set list on;
    select
        rdb$generator_name as seq_name,
        rdb$initial_value as seq_init,
        rdb$generator_increment as seq_incr,
        rdb$description as blob_id
    from rdb$generators
    where rdb$system_flag is distinct from 1;
"""

fbk_file = temp_file('tmp_core_5855.fbk')
fdb_file = temp_file('tmp_core_5855.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path, fdb_file: Path):
    with act.connect_server() as srv:
        srv.database.backup(database=act.db.db_path, backup=fbk_file)
        srv.wait()
        srv.database.restore(backup=fbk_file, database=fdb_file)
        srv.wait()
    act.expected_stdout = expected_stdout
    act.isql(switches=[act.get_dsn(fdb_file)], input=test_script, connect_db=False)
    assert act.clean_stdout == act.clean_expected_stdout
