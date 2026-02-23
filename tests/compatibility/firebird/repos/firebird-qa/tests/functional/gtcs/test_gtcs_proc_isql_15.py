#coding:utf-8

"""
ID:          gtcs.proc-isql-15
TITLE:       gtcs-proc-isql-15
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_15.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_15
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set term ^;
    create procedure proc_insert (a char(5), b char(20), c char(6), d smallint, e char(15)) as
    begin
    insert into p values (:a, :b, :c, :d, :e);
    end
    ^
    set term ;^
    select 'point-1' msg, p.* from p;
    execute procedure proc_insert 'P7', 'Widget', 'Pink', 23, 'Hoboken';
    select 'point-2' msg, p.* from p;
"""

act = isql_act('db', test_script, substitutions=[('={3,}', ''), ('[ \t]+', ' ')])

expected_stdout = """
    MSG     PNO    PNAME                COLOR        WEIGHT CITY
    point-1 P1     Nut                  Red              12 London
    point-1 P2     Bolt                 Green            17 Paris
    point-1 P3     Screw                Blue             17 Rome
    point-1 P4     Screw                Red              14 London
    point-1 P5     Cam                  Blue             12 Paris
    point-1 P6     Cog                  Red              19 London

    MSG     PNO    PNAME                COLOR        WEIGHT CITY
    point-2 P1     Nut                  Red              12 London
    point-2 P2     Bolt                 Green            17 Paris
    point-2 P3     Screw                Blue             17 Rome
    point-2 P4     Screw                Red              14 London
    point-2 P5     Cam                  Blue             12 Paris
    point-2 P6     Cog                  Red              19 London
    point-2 P7     Widget               Pink             23 Hoboken
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
