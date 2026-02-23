#coding:utf-8

"""
ID:          gtcs.external-file-03
FBTEST:      functional.gtcs.external_file_03_d
TITLE:       Test for external table with field of SMALLINT datatype
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/EXT_REL_0_3_D.script

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

tmp_ext_file = temp_file('tmp_gtcs_external_file_03_d.dat')

expected_stderr = """
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
"""

expected_stdout = """
    01: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
    F01 -32768
    F01 -1
    F01 0
    F01 1
    F01 32767
    Records affected: 5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_ext_file: Path):
    
    sql_cmd = f'''
        create table ext_table external file '{str(tmp_ext_file)}' (f01 smallint);
        commit;
        insert into ext_table (f01) values ( 32767);
        insert into ext_table (f01) values (-32768);
        insert into ext_table (f01) values (1);
        insert into ext_table (f01) values (-1);
        insert into ext_table (f01) values (0);

        -- All subsequent statements must FAIL:
        insert into ext_table (f01) values ( 32768);
        insert into ext_table (f01) values (-32769);
        insert into ext_table (f01) values (0xF0000000);  -- -268435456
        insert into ext_table (f01) values (0x0F0000000); -- 4026531840
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
