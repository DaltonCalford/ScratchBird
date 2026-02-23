#coding:utf-8

"""
ID:          issue-8822
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8822
TITLE:       Some procedures containing LIST aggregate function are not restored in Firebird 6.0 if backup was made in 5.0
DESCRIPTION:
    Test uses backup of database that was created in ODS 13.1 (FB 5.0.4.x) and contains SP with source from the ticket.
    We extract this .fbk from .zip and try to restore from it using services API. No errors must occur.
    Finally, we query some info for this SP from RDB$PROCEDURES and run SP. No error must raise.
NOTES:
    [05.01.2026] pzotov
    Confirmed bug on 6.0.0.1387, got:
        Error while parsing procedure "PUBLIC"."SOME_PROC"'s BLR
        -bad BLR -- invalid stream
        -Exiting before completion due to errors
        (335544876, 335544382, 336330835)
    Checked on 6.0.0.1389-f784b93.
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

tmp_fbk = temp_file('gh_8822-ods13_1.fbk')
tmp_fdb = temp_file('tmp_restore_8822.fdb')

#------------------------------------------------------------------
# Callback function: capture output of ERROR messages in restore (output must be empty):
def print_log(line: str) -> None:
    if ( s:= line.strip()):
        if 'ERROR' in s:
            print(s)
#------------------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, capsys):

    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8822-ods13_1.zip', at = 'gh_8822-ods13_1.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())
    #
    restore_log = []
    test_sql = """
        set list on;
        set blob all;
        select
             p.rdb$procedure_name
            ,p.rdb$procedure_inputs
            ,p.rdb$procedure_outputs
            ,p.rdb$valid_blr
        from rdb$database r
        left join rdb$procedures p on p.rdb$procedure_name = upper('some_proc');
        select sign(count(*)) from some_proc(0);
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
        RDB$PROCEDURE_NAME SOME_PROC
        RDB$PROCEDURE_INPUTS 1
        RDB$PROCEDURE_OUTPUTS 2
        RDB$VALID_BLR 1
        SIGN 1
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
