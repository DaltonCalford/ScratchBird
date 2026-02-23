#coding:utf-8

"""
ID:          issue-5801
ISSUE:       5801
TITLE:       Crash on 3.0 and 4.0 when DB contains database-level trigger
DESCRIPTION:
JIRA:        CORE-5533
FBTEST:      bugs.core_5533
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop trigger trg_tx_start';
        when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    recreate table tlog (ID integer default current_transaction, msg varchar(20));

    set term ^;
    create trigger trg_tx_start inactive on transaction start position 0
    as
    begin
      execute statement ('insert into tlog(msg) values(?)') ('Tx start');
    end
    ^
    set term ;^
    commit;

    set autoddl off;
    select count(distinct id) id_distinct_count_0 from tlog;
    alter trigger trg_tx_start active;
    commit;

    set term ^;
    execute block as
        declare c int;
    begin
        begin
            execute statement 'drop trigger trg_tx_start';
        when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    select count(distinct id) id_distinct_count_1 from tlog;
    commit;
    select count(distinct id) id_distinct_count_2 from tlog;
    quit;

"""

act = isql_act('db', test_script)

expected_stdout = """
    ID_DISTINCT_COUNT_0             0
    ID_DISTINCT_COUNT_1             1
    ID_DISTINCT_COUNT_2             1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

