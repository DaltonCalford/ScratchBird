#coding:utf-8

"""
ID:          gtcs.proc-isql-02
TITLE:       gtcs-proc-isql-02
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_02.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_02
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set bail on;
    set term ^;
    create procedure proc2  returns (a varchar(5), b varchar(5),c integer) as
    begin
       for
           select *  from sp where pno = 'P5'
           into :a, :b, :c
       do
           suspend;
    end
    ^
    set term ;^
    execute procedure proc2;

    set count on;
    --set echo on;
    select 'point-1' msg, p.* from proc2 p;
    select 'point-2' msg, max(p.c) from proc2 p;
    select 'point-3' msg, p.a from proc2 p;
    select 'point-4' msg, p.* from proc2 p order by p.c;
    select 'point-5' msg, p.a, avg(p.c) from proc2 p group by p.a having avg(p.c) > 350;
    select 'point-6' msg, p.a, avg(p.c) from proc2 p group by p.a;
    select 'point-7' msg, p.a, p.c from proc2 p where p.c = (select avg(x.c) from proc2 x);

"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    A      B                 C
    S4     P5              400

    MSG     A      B                 C
    point-1 S4     P5              400
    Records affected: 1

    MSG              MAX
    point-2          400
    Records affected: 1

    MSG     A
    point-3 S4
    Records affected: 1

    MSG     A      B                 C
    point-4 S4     P5              400
    Records affected: 1

    MSG     A                        AVG
    point-5 S4                       400
    Records affected: 1

    MSG     A                        AVG
    point-6 S4                       400
    Records affected: 1

    MSG     A                 C
    point-7 S4              400
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
