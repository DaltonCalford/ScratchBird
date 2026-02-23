#coding:utf-8

"""
ID:          issue-5573
ISSUE:       5573
TITLE:       Validation could read after the end-of-file when handle multifile database
DESCRIPTION:
JIRA:        CORE-5295
FBTEST:      bugs.core_5295
NOTES:
    [13.12.2023] pzotov
    Adjusted substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see error messages produced by 'gfix -v'
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(from_backup='core5295.fbk')

act = python_act('db', substitutions=[('[ \t]+', ' '), ('^((?!checked_size|[Ee]rror|[Rr]eached).)*$', '')])

fbk_file = temp_file('tmp_core_5295.fbk')
fdb_file_1 = temp_file('tmp_core_5295-1.fdb')
fdb_file_2 = temp_file('tmp_core_5295-2.db1')

@pytest.mark.version('>=2.5.6')
def test_1(act: Action, fbk_file: Path, fdb_file_1: Path, fdb_file_2: Path):
    with act.connect_server() as srv:
        srv.database.backup(database=act.db.db_path, backup=fbk_file)
        srv.wait()
        srv.database.restore(backup=fbk_file,
                             database=[fdb_file_1, fdb_file_2],
                             db_file_pages=[100000])
        srv.wait()

    # Only 'gfix -v' raised error. Online validation works fine:
    act.gfix(switches=['-v', act.get_dsn(fdb_file_1)], combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
