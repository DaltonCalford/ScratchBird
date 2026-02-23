#coding:utf-8

"""
ID:          gtcs.proc-isql-18
TITLE:       gtcs-proc-isql-18
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_18.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_18
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set term ^;
    create procedure proc_select_insert as
    begin
        insert into sp(sno) select sno from s where sno not in (select sno from sp);
    end
    ^
    set term ;^
    select 'point-1' as msg, p.* from sp p;
    execute procedure proc_select_insert;
    select 'point-2' as msg, p.* from sp p;
    rollback;
    select 'point-3' as msg, p.* from sp p;
    execute procedure proc_select_insert;
    select 'point-4' as msg, p.* from sp p;
"""

act = isql_act('db', test_script, substitutions=[('={3,}', ''), ('[ \t]+', ' ')])

expected_stdout = """
    MSG     SNO    PNO             QTY
    point-1 S1     P1              300
    point-1 S1     P3              400
    point-1 S2     P1              300
    point-1 S2     P2              400
    point-1 S4     P4              300
    point-1 S4     P5              400

    MSG     SNO    PNO             QTY
    point-2 S1     P1              300
    point-2 S1     P3              400
    point-2 S2     P1              300
    point-2 S2     P2              400
    point-2 S4     P4              300
    point-2 S4     P5              400
    point-2 S3     <null>       <null>
    point-2 S5     <null>       <null>

    MSG     SNO    PNO             QTY
    point-3 S1     P1              300
    point-3 S1     P3              400
    point-3 S2     P1              300
    point-3 S2     P2              400
    point-3 S4     P4              300
    point-3 S4     P5              400

    MSG     SNO    PNO             QTY
    point-4 S1     P1              300
    point-4 S1     P3              400
    point-4 S2     P1              300
    point-4 S2     P2              400
    point-4 S4     P4              300
    point-4 S4     P5              400
    point-4 S3     <null>       <null>
    point-4 S5     <null>       <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
