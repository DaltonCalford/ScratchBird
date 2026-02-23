#coding:utf-8

"""
ID:          gtcs.proc-isql-04
TITLE:       gtcs-proc-isql-04
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_04.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_04
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set bail on;
    set term ^;
    create procedure proc4 returns (a varchar(6), b varchar(5),c integer) as
    begin
        for
            select color, pno, weight
            from  p
            where weight = 19
            group by color, pno, weight
            into :a, :b, :c
        do
            suspend;
    end
    ^
    set term ;^

    execute procedure proc4 ;

    set count on;
    select 'point-1' msg, p.* from proc4 p;
    select 'point-2' msg, max(p.a) from proc4 p;
    select 'point-3' msg, p.c from proc4 p;
    select 'point-4' msg, p.a, p.c from proc4 p order by p.a;
    select 'point-5' msg, p.a, avg(p.c) from proc4 p group by p.a having avg(p.c) > 35;
    select 'point-6' msg, p.a, avg(p.c) from proc4 p group by p.a ;
    select 'point-7' msg, p.a, p.c from proc4 p where p.c = (select avg(x.c) from proc4 x);
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    A      B                 C
    Red    P6               19

    MSG     A      B                 C
    point-1 Red    P6               19
    Records affected: 1

    MSG     MAX
    point-2 Red
    Records affected: 1

    MSG                C
    point-3           19
    Records affected: 1

    MSG     A                 C
    point-4 Red              19
    Records affected: 1

    Records affected: 0

    MSG     A                        AVG
    point-6 Red                       19
    Records affected: 1

    MSG     A                 C
    point-7 Red              19
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
