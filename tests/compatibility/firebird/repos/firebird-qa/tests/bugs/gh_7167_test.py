#coding:utf-8

"""
ID:          issue-7167
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7167
TITLE:       Incorrect transliteration of field names in constraint violation errors
DESCRIPTION:
NOTES:
    [28.02.2023] pzotov
        Confirmed bug on 4.0.1.2692 Windows and Linux.
        NB: on Linux we have to write SQL script into file with encoding = cp1251
        and run it as script, otherwise issue not reproduced.
        Checked on Windows and Linux, builds 5.0.0.961, 4.0.3.2903 - all OK.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.894; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *
from pathlib import Path
import time

db = db_factory(charset = 'win1251')
act = python_act('db', substitutions = [('After line \\d+ in file .*', '')])
tmp_sql = temp_file('tmp_gh_7167.tmp.sql')

@pytest.mark.version('>=4.0.2')
def test_1(act: Action, tmp_sql: Path):
    test_sql = """
        create table "абв" (
            "аб" varchar(5) character set win1251,
            "вг" varchar(5) character set dos866,
            "де" varchar(5) character set utf8,
            constraint "уни" unique ("аб", "вг", "де")
        );
        insert into "абв" values ('аб','аб','аб');
        insert into "абв" values ('аб','аб','аб');
    """
    tmp_sql.write_bytes(test_sql.encode('cp1251'))

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 23000
        violation of PRIMARY or UNIQUE KEY constraint "уни" on table {SQL_SCHEMA_PREFIX}"абв"
        -Problematic key value is ("аб" = 'аб', "вг" = 'аб', "де" = 'аб')
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], charset = 'win1251', input_file = tmp_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
