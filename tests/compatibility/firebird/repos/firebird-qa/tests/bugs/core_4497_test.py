#coding:utf-8

"""
ID:          issue-4816
ISSUE:       4816
TITLE:       Regression in 3.0.x: wrong handling in FOR-cursor when NOT EXISTS( select from <VIEW> )
  statement is used to check results obtained from SP
DESCRIPTION:
JIRA:        CORE-4497
FBTEST:      bugs.core_4497
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_test as begin end;
    create or alter procedure z_pget as begin end;
    commit;

    create or alter view z_vdbg as select 1 as not_handled_agent_id from rdb$database;
    commit;

    recreate global temporary table z_gtt(id int, agent_id int) on commit delete rows;
    recreate table z_doc(id int, agent_id int);
    commit;

    insert into z_doc(id, agent_id) values (101, 7);
    commit;

    set term ^;
    create or alter procedure z_pget
    returns (
        clo_doc_id int,
        clo_agent_id int)
    as
    begin

        delete from z_gtt;
        insert into z_gtt select * from z_doc;

        for
            select f.id, f.agent_id
            from z_gtt f
            order by f.id
            into clo_doc_id, clo_agent_id
        do
            suspend;
    end
    ^

    create or alter procedure sp_test returns( doc_for_handling int, agent_for_handling int )
    as
      declare v_agent_id int;
    begin
        for
            select p.clo_doc_id, p.clo_agent_id from z_pget p
            into doc_for_handling, v_agent_id
        do begin
            agent_for_handling = null;
            if ( NOT exists(
                     select *
                     from z_vdbg v
                     where v.not_handled_agent_id =
                     (select h.agent_id
                                   from z_doc h
                                   where h.id= :doc_for_handling
                                   )
                           )

               ) then
            begin
               agent_for_handling = v_agent_id;
            end
            suspend;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;

    insert into z_doc(id, agent_id) values (100, 1);
    select * from sp_test;
    rollback;

    insert into z_doc(id, agent_id) values (102, 1);
    select * from sp_test;
    rollback;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DOC_FOR_HANDLING                100
    AGENT_FOR_HANDLING              <null>

    DOC_FOR_HANDLING                101
    AGENT_FOR_HANDLING              7


    DOC_FOR_HANDLING                101
    AGENT_FOR_HANDLING              7

    DOC_FOR_HANDLING                102
    AGENT_FOR_HANDLING              <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

