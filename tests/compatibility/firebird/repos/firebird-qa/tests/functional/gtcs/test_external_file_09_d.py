#coding:utf-8

"""
ID:          gtcs.external-file-09
FBTEST:      functional.gtcs.external_file_09_d
TITLE:       Test for external table with field of DATE datatype
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/EXT_REL_0_9_D.script
NOTES:
  [31.07.2022] pzotov
  FB config must allow creation of external tables, check parameter 'ExternalFileAccess'.
  Otherwise: SQLSTATE = 28000 / Use of external file at location ... is not allowed ...
  Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.591
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' '), ('(INPUT|OUTPUT)\\s+message .*', ''), (':\\s+(name:|table:)\\s+.*', '') ])

tmp_ext_file = temp_file('tmp_gtcs_external_file_09_date.dat')

expected_stderr = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "29-feb-9999"
"""

expected_stdout = """
    01: sqltype: 570 SQL DATE Nullable scale: 0 subtype: 0 len: 4
    F01 0001-01-01
    F01 1994-06-28
    F01 2001-09-01
    F01 2004-02-29
    F01 9999-12-31
    Records affected: 5

    THIS_DAY_COUNT 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_ext_file: Path):
    
    sql_cmd = f'''
        create table ext_table external file '{str(tmp_ext_file)}' (f01 date);
        commit;

        insert into ext_table (f01) values ('28-June-94');
        insert into ext_table (f01) values ('29-feb-4');
        insert into ext_table (f01) values ('1-september-1');
        insert into ext_table (f01) values ('1-january-0001');
        insert into ext_table (f01) values ('31-december-9999');
        insert into ext_table (f01) values (current_date);
        insert into ext_table (f01) values (current_timestamp);
        insert into ext_table (f01) values ('29-feb-9999');
        commit;
        set list on;
        set count on;
        set sqlda_display on;
        select * from ext_table where f01 <> current_date order by f01;
        set count off;
        set sqlda_display off;
        select count(*) as this_day_count from ext_table where f01=current_date;
        commit;
        drop table ext_table;
        exit;
    '''

    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr

    act.isql(switches=['-q'], input = sql_cmd )

    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
