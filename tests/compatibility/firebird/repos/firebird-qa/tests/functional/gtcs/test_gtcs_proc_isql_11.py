#coding:utf-8

"""
ID:          gtcs.proc-isql-11
TITLE:       gtcs-proc-isql-11
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_11.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_11
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set bail on;
    set term ^;
    create procedure proc11 returns (a varchar(5), b varchar(21), c integer) as
    begin
        for select p.pno,pname, count(*) from p, sp where p.pno = sp.pno group by p.pno, pname
        into :a, :b, :c
        do suspend;
    end
    ^
    set term ;^

    execute procedure proc11 ;

    set count on;
    select 'point-1' msg, p.* from proc11 p;
    select 'point-2' msg, max(p.a) from proc11 p;
    select 'point-3' msg, p.c from proc11 p;
    select 'point-4' msg, p.c, p.a from proc11 p order by p.c;
    select 'point-5' msg, p.a, avg(p.c) from proc11 p group by a having avg(p.c) > 1;
    select 'point-6' msg, p.a, avg(p.c) from proc11 p group by p.a ;
    select 'point-7' msg, p.a, p.c from proc11 p where p.c > (select avg(x.c) from proc11 x);
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    A      B                                C
    P1     Nut                              2

    MSG     A      B                                C
    point-1 P1     Nut                              2
    point-1 P2     Bolt                             1
    point-1 P3     Screw                            1
    point-1 P4     Screw                            1
    point-1 P5     Cam                              1
    Records affected: 5

    MSG     MAX
    point-2 P5
    Records affected: 1

    MSG                C
    point-3            2
    point-3            1
    point-3            1
    point-3            1
    point-3            1
    Records affected: 5

    MSG                C A
    point-4            1 P2
    point-4            1 P3
    point-4            1 P4
    point-4            1 P5
    point-4            2 P1
    Records affected: 5

    MSG     A                        AVG
    point-5 P1                         2
    Records affected: 1

    MSG     A                        AVG
    point-6 P1                         2
    point-6 P2                         1
    point-6 P3                         1
    point-6 P4                         1
    point-6 P5                         1
    Records affected: 5

    MSG     A                 C
    point-7 P1                2
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
