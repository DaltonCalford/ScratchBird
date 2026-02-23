#coding:utf-8

"""
ID:          gtcs.proc-isql-19
TITLE:       gtcs-proc-isql-19
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_ISQL_19.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_proc_isql_19
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set term ^;
    create procedure  proc_select_insert2 as
    declare variable t varchar(5);
    begin
        for select sno from s where sno not in
        (select sno from sp) into :t do
        begin
          insert into sp(sno) values (:t);
        end
    end
    ^
    set term ;^
    select 'result-1' as msg, p.* from sp p;
    execute procedure proc_select_insert2;
    select 'result-2' as msg, p.* from sp p;
"""

act = isql_act('db', test_script, substitutions=[('={3,}', ''), ('[ \t]+', ' ')])

expected_stdout = """
    MSG      SNO    PNO             QTY
    result-1 S1     P1              300
    result-1 S1     P3              400
    result-1 S2     P1              300
    result-1 S2     P2              400
    result-1 S4     P4              300
    result-1 S4     P5              400

    MSG      SNO    PNO             QTY
    result-2 S1     P1              300
    result-2 S1     P3              400
    result-2 S2     P1              300
    result-2 S2     P2              400
    result-2 S4     P4              300
    result-2 S4     P5              400
    result-2 S3     <null>       <null>
    result-2 S5     <null>       <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
