#coding:utf-8

"""
ID:          issue-7715
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7715
TITLE:       Alternative String Literals and multibyte (UTF8) alternatives
DESCRIPTION: 
NOTES:
    [02.10.2023] pzotov
    Confirmed bug on 5.0.0.1235 ("token unknown").
    Checked on 6.0.0.65 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')
act = python_act('db')

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    with act.db.connect(charset = 'utf8') as con:
        cur = con.cursor()
        cur.execute("select q'ΔΛορεμ ιπσθμ δολορ σιτ αμετΔ' from rdb$database")
        for r in cur:
            print(r[0])

    with act.db.connect(charset = 'win1251') as con:
        cur = con.cursor()
        cur.execute("select q'ЁЛорем ипсум долор сит аметЁ' from rdb$database")
        for r in cur:
            print(r[0])


    with act.db.connect(charset = 'win1250') as con:
        cur = con.cursor()
        cur.execute("select q'ŰLórum ipse mint süke kenző títékerŰ' from rdb$database")
        for r in cur:
            print(r[0])

    act.expected_stdout = """
        Λορεμ ιπσθμ δολορ σιτ αμετ
        Лорем ипсум долор сит амет
        Lórum ipse mint süke kenző títéker
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
