#coding:utf-8

"""
ID:          issue-6357
ISSUE:       6357
TITLE:       Regression: FB3 throws "Datatypes are not comparable in expression" in procedure parameters
DESCRIPTION:
JIRA:        CORE-6108
FBTEST:      bugs.core_6108
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    2019-03-01 00:00:00
"""

proc_ddl = """
    create or alter procedure test_proc ( a_dts timestamp) returns ( o_dts timestamp) as
    begin
        o_dts = a_dts;
        suspend;
    end
"""

@pytest.mark.version('>=2.5')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        con.execute_immediate(proc_ddl)
        con.commit()
        c = con.cursor()
        for row in c.execute("select o_dts from test_proc('2019-'|| COALESCE( ?, 1) ||'-01' )", [3]):
            print(row[0])
    #
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

