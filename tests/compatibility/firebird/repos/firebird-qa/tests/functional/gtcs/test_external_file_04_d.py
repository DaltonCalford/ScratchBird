#coding:utf-8

"""
ID:          gtcs.external-file-04-A
FBTEST:      functional.gtcs.external_file_04_d
TITLE:       est for external table with field of INTEGER datatype
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/EXT_REL_0_4_D.script
NOTES:
  [31.07.2022] pzotov
  FB config must allow creation of external tables, check parameter 'ExternalFileAccess'.
  Otherwise: SQLSTATE = 28000 / Use of external file at location ... is not allowed ...
  Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.591
"""

from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' '), ('(INPUT|OUTPUT)\\s+message .*', ''), (':\\s+(name:|table:)\\s+.*', '') ])

tmp_ext_file = temp_file('tmp_gtcs_external_file_04_d.dat')

expected_stderr = """
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
"""

expected_stdout = """
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    F01 -2147483648
    F01 -1
    F01 0
    F01 1
    F01 2147483647
    Records affected: 5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_ext_file: Path):
    
    sql_cmd = f'''
        create table ext_table external file '{str(tmp_ext_file)}' (f01 int);
        commit;
        insert into ext_table (f01) values ( 2147483647);
        insert into ext_table (f01) values (-2147483648);
        insert into ext_table (f01) values (1);
        insert into ext_table (f01) values (-1);
        insert into ext_table (f01) values (0);

        -- All subsequent statements must FAIL:
        insert into ext_table (f01) values (0x7FFFFFFFFFFFFFFF);
        insert into ext_table (f01) values (0x8000000000000000);
        commit;
        set list on;
        set count on;
        set sqlda_display on;
        select * from ext_table order by f01;
        commit;
        drop table ext_table;
        exit;
    '''

    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr

    act.isql(switches=['-q'], input = sql_cmd )

    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
