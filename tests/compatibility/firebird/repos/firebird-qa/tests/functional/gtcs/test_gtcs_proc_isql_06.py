#coding:utf-8

"""
ID:          gtcs.proc-isql-06
TITLE:       gtcs-proc-isql-06
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_06.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_06
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set bail on;
    set term ^;
    create procedure proc6 returns (a varchar(20), b integer) as
    begin
        for
            select pname,weight
            from p
            where weight > (select (avg(weight)+3) from p)
            into :a, :b
        do
            suspend;
    end
    ^
    set term ; ^

    execute procedure proc6 ;

    set count on;
    select 'point-1' msg, p.* from proc6 p;
    select 'point-2' msg, max(p.a) from proc6 p;
    select 'point-3' msg, p.b from proc6 p;
    select 'point-4' msg, p.a, p.b from proc6 p order by a;
    select 'point-5' msg, p.a, avg(p.b) from proc6 p group by a having avg(p.b) > 35;
    select 'point-6' msg, p.a, avg(p.b) from proc6 p group by a ;
    select 'point-7' msg, p.a , b from proc6 p where b = (select avg(x.b) from proc6 x);
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    A                               B
    Cog                            19

    MSG     A                               B
    point-1 Cog                            19
    Records affected: 1

    MSG     MAX
    point-2 Cog
    Records affected: 1

    MSG                B
    point-3           19
    Records affected: 1

    MSG     A                               B
    point-4 Cog                            19
    Records affected: 1

    Records affected: 0

    MSG     A                                      AVG
    point-6 Cog                                     19
    Records affected: 1

    MSG     A                               B
    point-7 Cog                            19
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
