#coding:utf-8

"""
ID:          issue-3017
ISSUE:       3017
TITLE:       Introducer (_charset) problems with monitoring and persistent modules
DESCRIPTION:
  Usage of introducer (_charset ) is problematic due to usage of different character sets
  in a single command. The problems are different from version to version, and may be
  transliteration error, malformed string or just unexpected things.
JIRA:        CORE-2607
FBTEST:      bugs.core_2607
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

act = python_act('db')

expected_stdout = """
    select mon$sql_text from mon$statements s where s.mon$sql_text containing '_dos850'
    select _dos850 X'313233C3A1C3A9343536' from rdb$database
"""

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    act.expected_stdout = expected_stdout
    with act.db.connect() as con1:
        c1 = con1.cursor()
        with act.db.connect() as con2:
            c2 = con2.cursor()
            c1.execute("select _dos850 '123áé456' from rdb$database")
            c2.execute("select mon$sql_text from mon$statements s where s.mon$sql_text containing '_dos850'")
            for r in c2:
                print(r[0])
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout



