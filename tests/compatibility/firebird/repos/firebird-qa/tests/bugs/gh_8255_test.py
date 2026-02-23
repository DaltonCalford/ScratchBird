#coding:utf-8

"""
ID:          issue-8255
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8255
TITLE:       Catch possible stack overflow when preparing and compiling user statements
DESCRIPTION:
    Test generates SQL like 'select 1+1+1....+1 from rdb$database' and tries to execte it.
    Exception 'SQLSTATE = HY001 / Stack overflow' must raise instead of crash (that was before fix).
    Commits:
    * 4.x: https://github.com/FirebirdSQL/firebird/commit/04c586d4ea4bafb50818bcf7f46188afc67ab1c5 (20-sep-2024)
    * 5.x: https://github.com/FirebirdSQL/firebird/commit/f0670f90cc7d1fc93db22336fd43abc6d348e31e (18-sep-2024)
    * 6.x: https://github.com/FirebirdSQL/firebird/commit/6b445c0dc53f1c5778258bd673c0b61f6dd93a69 (20-sep-2024)
NOTES:
    [23.09.2024] pzotov
    Initially query contained expression of 15'000 terms ("1+1+1...+1") was used to check.
    This query causes 'stack overflow' only in FB 5.x and 6.x.
    But in FB 4.0.6.3156 it successfully COMPLETES calculation and issues result.
    For FB 4.x this 'threshold' is 16'287 (last number of terms where FB can evaluate result w/o 'stack overflow').
    Because of this, it was decided to increase number of terms to 100'000.

    ::: NB :::
    Fix currently exists only for Windows, see:
    https://github.com/FirebirdSQL/firebird/pull/8255#issuecomment-2354781108
    On Linux this query still crashes server.

    Checked on 6.0.0.466, 5.0.2.1513, 4.0.6.3156
"""

from pathlib import Path
import platform
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' '), ('After line \\d+.*', '')])

tmp_sql = temp_file('tmp_8255_non_ascii_ddl.sql')

@pytest.mark.skipif(platform.system() != 'Windows', reason='See ticket note: fix was only for Windows.')
@pytest.mark.version('>=4.0.6')
def test_1(act: Action, tmp_sql: Path, capsys):

    long_expr = '\n'.join( (
                              'select'
                             ,'+'.join( ('1') * 100000 )
                             ,'from rdb$database;'
                           )
                         )

    tmp_sql.write_bytes(long_expr.encode('utf-8'))

    act.isql(switches=['-q'], input_file=tmp_sql, combine_output = True, charset='win1251')

    act.expected_stdout = f"""
        Statement failed, SQLSTATE = HY001
        Stack overflow. The resource requirements of the runtime stack have exceeded the memory available to it.
    """
    assert act.clean_stdout == act.clean_expected_stdout

