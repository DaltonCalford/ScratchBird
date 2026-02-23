#coding:utf-8

"""
ID:          gtcs.select-delete-isql
TITLE:       Test for select from SP that deletes record after its output
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/SELECT_DELETE_ISQL.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
FBTEST:      functional.gtcs.gtcs_select_delete_isql
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='gtcs_sp1.fbk')

test_script = """
    set bail on;
    set term ^;
    create procedure proc returns (x varchar(5)) as
    begin
        for select sno from sp into :x
        do begin
            suspend;
            delete from s where sno = :x;
        end
    end
    ^
    create procedure proc1 returns (x varchar(5)) as
    begin
        for select sno from sp into :x
        do begin
            suspend;
        end
        delete from s where sno = :x;
    end
    ^
    create procedure proc2 returns (x varchar(5)) as
    begin
        for select sno from sp into :x
        do begin
            delete from s where sno = :x;
            suspend;
        end
    end
    ^
    set term ;^

    set count on;

    select 'point-1' msg, p.* from proc p;
    select 'point-2' msg, s.* from s;
    rollback;
    select 'point-3' msg, p.* from proc1 p;
    select 'point-4' msg, s.* from s;
    rollback;
    select 'point-5' msg, p.* from proc2 p;
    rollback;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    MSG     X
    point-1 S1
    point-1 S1
    point-1 S2
    point-1 S2
    point-1 S4
    point-1 S4
    Records affected: 6

    MSG     SNO    SNAME                      STATUS CITY
    point-2 S3     Blake                          30 Paris
    point-2 S5     Adams                          30 Athens
    Records affected: 2

    MSG     X
    point-3 S1
    point-3 S1
    point-3 S2
    point-3 S2
    point-3 S4
    point-3 S4
    Records affected: 6

    MSG     SNO    SNAME                      STATUS CITY
    point-4 S1     Smith                          20 London
    point-4 S2     Jones                          10 Paris
    point-4 S3     Blake                          30 Paris
    point-4 S5     Adams                          30 Athens
    Records affected: 4

    MSG     X
    point-5 S1
    point-5 S1
    point-5 S2
    point-5 S2
    point-5 S4
    point-5 S4
    Records affected: 6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
