#coding:utf-8

"""
ID:          gtcs.proc-isql-05
TITLE:       gtcs-proc-isql-05
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_05.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_05
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set bail on;
    set term ^;
    create procedure proc5 returns (a varchar(20),  b integer) as
    begin
        for
            select pname, avg(weight)
            from p
            group by pname
            having avg(weight) > 18
            into :a, :b
        do
            suspend;
    end
    ^
    set term ; ^

    execute procedure proc5 ;

    set count on;
    select p.* from proc5 p;
    select 'point-1' msg, max(p.a) from proc5 p;
    select 'point-2' msg, p.b from proc5 p;
    select 'point-3' msg, p.a, p.b from proc5 p order by p.a;
    select 'point-4' msg, p.a, avg(p.b) from proc5 p group by p.a having avg(p.b) > 35;
    select 'point-5' msg, p.a, avg(p.b) from proc5 p group by p.a;
    select 'point-6' msg, p.a, p.b from proc5 p where p.b = (select avg(x.b) from proc5 x);

"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    A                               B
    Cog                            19

    A                               B
    Cog                            19
    Records affected: 1

    MSG     MAX
    point-1 Cog
    Records affected: 1

    MSG                B
    point-2           19
    Records affected: 1

    MSG     A                               B
    point-3 Cog                            19
    Records affected: 1

    Records affected: 0

    MSG     A                                      AVG
    point-5 Cog                                     19
    Records affected: 1

    MSG     A                               B
    point-6 Cog                            19
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
