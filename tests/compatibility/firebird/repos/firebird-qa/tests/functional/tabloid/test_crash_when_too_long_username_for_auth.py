#coding:utf-8

"""
TITLE:       crash on assert or memory corruption when too long username is used for authentication
NOTES:
    [19.07.2023] pzotov
    Confirmed problem on 3.x only (3.0.11.33690): server crashed, client got:
        Statement failed, SQLSTATE = 08004
        connection rejected by remote interface
    FB 4.0.3.2956 and 5.0.0.1093 (builds before 29-JUN-2023) issued "SQLSTATE = 28000 / Your user name and password..." but not crashed.
    After fix, all three FB issue "SQLSTATE = 08006 / Error occurred during login, please check server firebird.log for details"
    Checked on 3.0.11.33965 -- all OK.

    [14.07.2024] pzotov
    Customized for run against dev build after Dimitry Sibiryakov request.
    Dev build issues status vector containing TWO elements:
        335545106 ==> Error occurred during login, please check server firebird.log for details
        335544882 ==> Login name too long (@1 characters, maximum allowed @2)
    We can filter out its second item..
"""
import locale
import re
from difflib import unified_diff

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act( 'db', substitutions=[('[ \t]+',' '), ('.*Login name too long.*', '')] )

@pytest.mark.version('>=3.0.11')
def test_1(act: Action, capsys):

    TOO_LONG_USR = 'u2345678901234567890123456789012345678901234567890123456789012345678901'
    #                        1         2         3         4         5         6         7
    try:
        with act.db.connect(user = TOO_LONG_USR, password = 'qwe', charset = 'win1251'):
            pass
    except DatabaseError as e:
        # ACHTUNG: dev-build will raise error with TWO gdscodes: [335545106, 335544882].
        #   335545106 ==> Error occurred during login, please check server firebird.log for details
        #   335544882 ==> Login name too long (@1 characters, maximum allowed @2)
        # We have to check only first of them. DO NOT iterate through gds_codes tuple!
        print( e.gds_codes[0] ) 
        print( e.__str__() )

    act.stdout = capsys.readouterr().out
    act.expected_stdout = """
        335545106
        Error occurred during login, please check server firebird.log for details
    """

    assert act.clean_stdout == act.clean_expected_stdout
