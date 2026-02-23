#coding:utf-8

"""
ID:          issue-7179
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7179
TITLE:       Wrong error message - "string right truncation. expected length NN, actual NN."
NOTES:
    [27.02.2023] pzotov
    Confirmed bug on 5.0.0.488 (date of build: 25-apr-2022).
    Checked on 5.0.0.959 - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 4, actual 5

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 4, actual 5

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 4, actual 20

    RESULT_04                       1234
    RESULT_05                       1234
    RESULT_06                       1234
    RESULT_07                       1234

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 2, actual 9

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 2, actual 9

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 4, actual 5

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 4, actual 5

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 4, actual 6

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 4, actual 6
"""

@pytest.mark.version('>=4.0.2')
def test_1(act: Action):
    test_script = """
        set list on;
        select cast(_win1252 '12345' collate win_ptbr as varchar(4) character set win1252) as result_01 from rdb$database;

        select cast(_utf8 '1234ç' as varchar(4) character set win1252) as result_02 from rdb$database;

        select cast(_win1252 '12345678901234567890' as varchar(4) character set utf8) as result_03 from rdb$database;

        select cast(_utf8 '1234 ' as varchar(4) character set win1252) as result_04 from rdb$database;

        select cast(_utf8 '1234                ' as varchar(4) character set win1252) as result_05 from rdb$database;

        select cast(_win1252 '1234 ' as varchar(4) character set utf8) as result_06 from rdb$database;

        select cast(_win1252 '1234                ' as varchar(4) character set utf8) as result_07 from rdb$database;

        select cast(_none '12345678ç' as varchar(2) character set utf8) as result_08 from rdb$database;

        select cast(_none '1234567ç8' as varchar(2) character set utf8) as result_09 from rdb$database;

        select cast(_none '1234ç' as varchar(4) character set utf8) as result_10 from rdb$database;

        select cast(_none '1234ç' as char(4) character set utf8) as result_11 from rdb$database;

        select cast(_none '1234ç' as varchar(4) character set win1252) as result_12 from rdb$database;

        select cast(_none '1234ç' as char(4) character set win1252) as result_13 from rdb$database;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], charset = 'utf8', input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
