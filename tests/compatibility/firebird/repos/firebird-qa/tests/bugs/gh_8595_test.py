#coding:utf-8

"""
ID:          issue-8595
ISSUE:       8595
TITLE:       Unable to restore database to Firebird 6.0 (with schemas) from ODS 13.1 if database contains views with system tables used in subqueries
DESCRIPTION:
    Test uses backup of database that was created in ODS 13.1 (FB 5.0.3.x) and contain view V_TEST with column defined as subquery to mon$attachments table:
        create view v_test as
        select (select count(*) from mon$attachments) as att_cnt
        from rdb$database;
    We extract this .fbk from .zip and try to restore from it using services API. No errors must occur
    Finally, we run query to the V_TEST. It must run w/o errors and return one row.
NOTES:
    [13.06.2025] pzotov
    Confirmed bug on 6.0.0.800 (got 'table "PUBLIC"."MON$ATTACHMENTS" is not defined').
    Checked on 6.0.0.834-a9a0f28.
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

tmp_fbk = temp_file('gh_8595-ods13_1.fbk')
tmp_fdb = temp_file('tmp_restore_8595.fdb')

#------------------------------------------------------------------
# Callback function: capture output of ERROR messages in restore (output must be empty):
def print_log(line: str) -> None:
    if ( s:= line.strip()):
        if 'ERROR' in s:
            print(s)
#------------------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, capsys):

    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8595-ods13_1.zip', at = 'gh_8595-ods13_1.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())
    #
    restore_log = []
    with act.connect_server(encoding_errors = locale.getpreferredencoding()) as srv:
        try:
            srv.database.restore(backup=tmp_fbk, database=tmp_fdb, flags=SrvRestoreFlag.REPLACE, verbose=True, callback = print_log)
            act.isql( switches=[ str(tmp_fdb), '-q' ], connect_db = False, input = 'set list on; select sign(att_cnt) as att_cnt_sign from v_test;', combine_output = True )
            print(act.stdout)
        except DatabaseError as e:
            print(f'Restore failed:')
            print(e.__str__())
            print(e.gds_codes)
        except Exception as x:
            print(x)

    act.expected_stdout = """
        ATT_CNT_SIGN 1
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
