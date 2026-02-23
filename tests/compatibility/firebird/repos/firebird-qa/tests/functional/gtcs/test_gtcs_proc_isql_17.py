#coding:utf-8

"""
ID:          gtcs.proc-isql-17
TITLE:       gtcs-proc-isql-17
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_17.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_17
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set term ^;
    create procedure insert_sno (sno varchar(5)) as
        declare c int;
    begin
        select count(*) from sp where sno = :sno into :c;
        if (c = 0 ) then
            insert into sp(sno) values(:sno);
    end
    ^
    set term ;^
    execute procedure insert_sno 'S10';
    select p.* from sp p;
"""

act = isql_act('db', test_script, substitutions=[('={3,}', ''), ('[ \t]+', ' ')])

expected_stdout = """
    SNO    PNO             QTY
    S1     P1              300
    S1     P3              400
    S2     P1              300
    S2     P2              400
    S4     P4              300
    S4     P5              400
    S10    <null>       <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
