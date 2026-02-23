#coding:utf-8

"""
ID:          issue-5430
ISSUE:       5430
TITLE:       create trigger fails with ambiguous field name between table B and table A error
DESCRIPTION:
JIRA:        CORE-5147
FBTEST:      bugs.core_5147
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Result: OK on build based on sources 18-03-2016 14:40
    -- (take sources from github, build No. was wrong - 32400).

    recreate table test_b (
       id integer not null,
       qty float,
       constraint pk_b primary key (id)
    );
    recreate table test_a (
       id integer not null,
       qty float,
       constraint pk_a primary key (id)
    );

    set term ^;
    create or alter trigger test_b_bi for test_b active before insert position 0 as
        declare oldqty float;
        declare newqty float;
    begin
       update test_a
          set qty = new.qty
       where
          id=new.id
       returning new.qty, old.qty
       into newqty, oldqty;

       rdb$set_context('USER_SESSION' ,'TRG_OLD_QTY', oldQty);
       rdb$set_context('USER_SESSION' ,'TRG_NEW_QTY', newQty);
    end
    ^
    set term ;^
    commit;

    insert into test_a values( 1,  3.1415926);
    insert into test_b(id, qty) values( 1, 2.718281828 );

    set list on;

    select
       rdb$get_context('USER_SESSION' ,'TRG_OLD_QTY') as trg_old_qty
       ,rdb$get_context('USER_SESSION' ,'TRG_NEW_QTY') as trg_new_qty
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TRG_OLD_QTY                     3.1415925
    TRG_NEW_QTY                     2.7182817
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

