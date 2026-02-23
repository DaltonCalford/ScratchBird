#coding:utf-8

"""
ID:          trigger.database.transaction-rollback
TITLE:       Trigger on rollback transaction
DESCRIPTION:
  Test trigger on rollback transaction
FBTEST:      functional.trigger.database.transactionrollback_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create sequence g;
    commit;

    create view v_check as
    select rdb$get_context('USER_SESSION', 'TRG_TX') as ctx
    from rdb$database;
    commit;

    set term ^;
    create or alter trigger trg_tx_rbak inactive on transaction rollback position 0 as
    begin
      --- nop ---
    end
    ^
    set term ;^
    commit;

    recreate table test(id int);
    commit;

    recreate table log (main_app_tx int,  db_trigger_tx int default current_transaction, add_info varchar(80) );
    commit;

    set term ^;
    create or alter trigger trg_tx_rbak inactive on transaction rollback position 0 as
    begin
        rdb$set_context('USER_SESSION', 'TRG_TX', gen_id(g,1));
    end
    ^
    set term ;^
    commit;

    set autoddl off;
    alter trigger trg_tx_rbak active;
    commit;

    select gen_id(g,0) as curr_g, v.* from v_check v; -- 0, <null>

    rollback; -- this should increase value of sequence 'g' by 1 and assign new value to context var. 'TRG_TX'

    set transaction no wait;
    select gen_id(g,0) as curr_g, v.* from v_check v; -- 1, 1 (the same! becase Tx START should not be watched by trg_tx_rbak)

    commit;
    select gen_id(g,0) as curr_g, v.* from v_check v; -- 1, 1 (the same! because COMMIT should not be watched by trg_tx_rbak)

    set term ^;
    create or alter trigger trg_tx_rbak active on transaction rollback position 0 as
    begin
        rdb$set_context('USER_SESSION', 'TRG_TX', gen_id(g,1234)/0);
    end
    ^
    set term ;^
    commit;

    -- this should increase value of sequence 'g' by 1234 but context var. 'TRG_TX'
    -- will store old value because of zero-divide exception
    rollback;

    -- NB: client should NOT get exception (zero divide) from trigger which is defined for __ROLLBACK__ event!

    select gen_id(g,0) as curr_g, v.* from v_check v; -- 1235, 1
"""

act = isql_act('db', test_script)

expected_stdout = """
    CURR_G                          0
    CTX                             <null>

    CURR_G                          1
    CTX                             1

    CURR_G                          1
    CTX                             1

    CURR_G                          1235
    CTX                             1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
