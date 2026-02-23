#coding:utf-8

"""
ID:          issue-2229
ISSUE:       2229
TITLE:       Statement level read consistency in read-committed transactions
DESCRIPTION:
    We create table with single column and inspect it TWICE by this query: 'select max(x) from test'.
    Both queries are in the single procedure, but they are separated by autonomous transaction.
    Initially (before 1st query) this table has one record with value=1, so the first query will return 1.
    Then autonomous transaction inserts into this table 10 rows with incremental values.
    After this, query runs again.

    Procedure is executed within READ COMMITTED transations.

    If current transaction was started as READ CONSISTENCY then 2nd query must return the same value as 1st.
    Otherwise 2nd query return DIFFERENT (last of newly added) value and output column MAX_X will differ.

    Checked on 4.0.0.1573.

    ::: NB :::
    It is stated (in doc\\README.read_consistency.md ) that "In the future versions of Firebird old kinds of read-committed transactions could be removed".
    But for now we can suppose that at least in FB 4.x family these modes will be preserved and we can use them beside new (READ CONSISTENCY) mode.
    This means that we can check in this test BOTH modes and compare results, i.e.:
    1. Start Tx in READ CONSISTENCY mode, get result_1; commit;
    2. Start Tx in READ RECORD_VERSION, get result_2 - and it must differ from result_1.
    For this test could start Tx in READ RECORD_VERSION mode, parameter ReadConsistency in firebird.conf must be set to 0 (ZERO).
    THIS VALUE DIFFERS FROM DEFAULT, but it is not a problem for other major FB-versions: config is prepared separately for each of them.
    So, if in the future some major FB version will exclude RECORD_VERSION at all we can prepare new section of this test
    which will assume that there is no such parameter (ReadConsistency) in firebird.conf and check only one isolation mode.

    :::::::::::::::::::::::::::::::::::::::: NB ::::::::::::::::::::::::::::::::::::
    18.08.2020. FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
    statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
    gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
    See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
    ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt

    Because of this, it was decided to replace 'alter sequence restart...' with subtraction of two gen values:
    c = gen_id(<g>, -gen_id(<g>, 0)) -- see procedure sp_restart_sequences.
NOTES:
[3.11.2021] pcisar
  This test fails for v4.0.0.2496 (4.0 final)
JIRA:        CORE-5953
FBTEST:      bugs.core_5953
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- drop dependencies if any:
    create or alter procedure sp_restart_sequences as begin end;
    create or alter view v_check as select 1 as isol_level, 1 as max_x from rdb$database;
    commit;
    create or alter procedure sp_run_heavy_query returns( dts timestamp, max_x int ) as begin end;

    recreate sequence g;
    recreate table test(x int, constraint test_pk primary key(x) using descending index test_x_desc_pk);
    commit;

    set term ^;
    alter procedure sp_run_heavy_query returns( dts timestamp, max_x int ) as
    begin

        execute statement 'select max(x) from test' into max_x;
        dts='now';
        suspend;

        in autonomous transaction do
        begin
            insert into test(x) select gen_id(g,1) from rdb$types rows 10;
        end

        execute statement 'select max(x) from test' into max_x;
        dts='now';

        suspend;
    end
    ^
    create or alter procedure sp_restart_sequences as
        declare c bigint;
    begin
        c = gen_id(g, -gen_id(g, 0));
    end
    ^
    set term ;^
    commit;

    recreate view v_check as
    select
        t.mon$isolation_mode as mon_isol_mode
       ,rdb$get_context('SYSTEM', 'ISOLATION_LEVEL') as ctx_isol_level
       ,d.max_x
    from mon$transactions t
    left join sp_run_heavy_query d on 1=1
    where t.mon$transaction_id = current_transaction;

    commit;

    insert into test values( gen_id(g,1) );
    commit;

    set list on;

    set transaction READ COMMITTED READ CONSISTENCY lock timeout 1;
    select * from v_check;

    delete from test;

    execute procedure sp_restart_sequences;
    commit;

    insert into test values( gen_id(g,1)  );
    commit;

    set transaction READ COMMITTED RECORD_VERSION lock timeout 1;
    select * from v_check;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MON_ISOL_MODE                   4
    CTX_ISOL_LEVEL                  READ COMMITTED
    MAX_X                           1

    MON_ISOL_MODE                   4
    CTX_ISOL_LEVEL                  READ COMMITTED
    MAX_X                           1



    MON_ISOL_MODE                   2
    CTX_ISOL_LEVEL                  READ COMMITTED
    MAX_X                           1

    MON_ISOL_MODE                   2
    CTX_ISOL_LEVEL                  READ COMMITTED
    MAX_X                           11
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
