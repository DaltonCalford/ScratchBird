#coding:utf-8

"""
ID:          gtcs.external-file-06
FBTEST:      functional.gtcs.external_file_06_d
TITLE:       Test for external table with field of DOUBLE PRECISION datatype
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/EXT_REL_0_6_D.script
NOTES:
[03.03.2021] pzotov
  Added substitution for zero value ('F02') as result of evaluating exp(-745.1332192):
  on Windows number of digits in decimal representation more than on Linux for 1.
NOTES:
  [31.07.2022] pzotov
  1. FB config must allow creation of external tables, check parameter 'ExternalFileAccess'.
     Otherwise: SQLSTATE = 28000 / Use of external file at location ... is not allowed ...
  2. REMOVED evaluation of exp(-745.***) because results depends on CPU (intel vs AMD).
  Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.591
"""


from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' '), ('(INPUT|OUTPUT)\\s+message .*', ''), (':\\s+(name:|table:)\\s+.*', '') ])

tmp_ext_file = temp_file('tmp_gtcs_external_file_06_double_precision.dat')

expected_stderr = """
"""

expected_stdout = """
    01: sqltype: 480 DOUBLE Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 480 DOUBLE Nullable scale: 0 subtype: 0 len: 8
    F01 3.141592653589793
    F02 2.718281828459045
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_ext_file: Path):
    
    sql_cmd = f'''
        create domain dm_dp double precision;
        create table ext_table external file '{str(tmp_ext_file)}' (f01 dm_dp, f02 dm_dp);
        commit;
        insert into ext_table (f01, f02) values( pi(), exp(1) );
        commit;
        set list on;
        set count on;
        set sqlda_display on;
        select * from ext_table;
        commit;
        drop table ext_table;
        exit;
    '''

    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr

    act.isql(switches=['-q'], input = sql_cmd )

    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
