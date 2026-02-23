#coding:utf-8

"""
ID:          gtcs.proc-isql-01
TITLE:       gtcs-proc-isql-01
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_01.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_01
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set bail on;

    set term ^;
    create procedure proc1  returns (a integer) as
    begin
        for
            select distinct max(qty)
            from sp where qty > 300
            into :a
        do
            suspend;
    end
    ^
    set term ; ^

    execute procedure proc1;

    set count on;
    select 'point-1' msg, p.* from proc1 p;
    select 'point-2' msg, max(p.a) from proc1 p;
    select 'point-3' msg, p.a from proc1 p;
    select 'point-4' msg, p.* from proc1 p order by p.a;
    select 'point-5' msg, p.a, avg(p.a) from proc1 p group by p.a having avg(p.a) > 350;
    select 'point-6' msg, p.a, avg(p.a) from proc1 p group by p.a ;
    select 'point-7' msg, p.a  from proc1 p where p.a = (select avg(x.a) from proc1 x);
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    A
    400

    MSG                A
    point-1          400
    Records affected: 1

    MSG              MAX
    point-2          400
    Records affected: 1

    MSG                A
    point-3          400
    Records affected: 1

    MSG                A
    point-4          400
    Records affected: 1

    MSG                A                   AVG
    point-5          400                   400
    Records affected: 1

    MSG                A                   AVG
    point-6          400                   400
    Records affected: 1

    MSG                A
    point-7          400
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
