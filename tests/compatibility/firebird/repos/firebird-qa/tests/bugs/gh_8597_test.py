#coding:utf-8

"""
ID:          issue-8597
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8597
TITLE:       Unable to restore database to Firebird 6.0 (with schemas) from ODS 13.1 if database contains views with system tables used in subqueries
DESCRIPTION:
    Test uses backup of database that was created in ODS 13.1 (FB 5.0.3.x) and contains 
    * standalone procedure + function
    * package with procedure and function.
    Every unit has IN-parameters defined as type of columns from some rdb$tables, and OUT parameters referencing to rdb$ domains.
    We extract this .fbk from .zip and try to restore from it using services API. No errors must occur
    Finally, we run every proc / func / packaged units - no error must raise.
NOTES:
    [13.06.2025] pzotov
    Confirmed bug on 6.0.0.835-3da8317:
        Error while parsing procedure "PUBLIC"."SP_TEST"'s BLR
        -column "RDB$RELATION_NAME" does not exist in table/view "PUBLIC"."RDB$RELATIONS"
    Checked on 6.0.0.835-0-b1fd7d8
"""

import pytest
import zipfile
import locale
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag, DatabaseError
import time

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

tmp_fbk = temp_file('gh_8597-ods13_1.fbk')
tmp_fdb = temp_file('tmp_restore_8597.fdb')

#------------------------------------------------------------------
# Callback function: capture output of ERROR messages in restore (output must be empty):
def print_log(line: str) -> None:
    if ( s:= line.strip()):
        if 'ERROR' in s:
            print(s)
#------------------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, capsys):

    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8597-ods13_1.zip', at = 'gh_8597-ods13_1.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())
    #
    restore_log = []
    test_sql = """
        set list on;
        set blob all;
        select p.o_id as proc_outcome from sp_test('rdb$database') p;
        select fn_test('rdb$database') as func_outcome from rdb$database;
        select p.o_id as pg_proc_outcome from pg_test.pg_sp_test('rdb$database') p;
        select pg_test.pg_fn_test('rdb$database') as pg_func_outcome  from rdb$database;
    """
    with act.connect_server(encoding_errors = locale.getpreferredencoding()) as srv:
        try:
            srv.database.restore(backup=tmp_fbk, database=tmp_fdb, flags=SrvRestoreFlag.REPLACE, verbose=True, callback = print_log)
            
            act.isql( switches=[ str(tmp_fdb), '-q' ], connect_db = False, input = test_sql, combine_output = True )
            print(act.stdout)
        except DatabaseError as e:
            print(f'Restore failed:')
            print(e.__str__())
            print(e.gds_codes)
        except Exception as x:
            print(x)

    act.expected_stdout = """
        PROC_OUTCOME 1
        FUNC_OUTCOME <standalone function>
        PG_PROC_OUTCOME 2
        PG_FUNC_OUTCOME <packaged_function>
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
