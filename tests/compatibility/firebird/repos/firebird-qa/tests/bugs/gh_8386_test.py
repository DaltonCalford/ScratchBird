#coding:utf-8

"""
ID:          issue-8386
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8386
TITLE:       Crash when creating index on table that uses UDR and ParallelWorkers > 1
DESCRIPTION: 
NOTES:
    [18.01.2025] pzotov
    Confirmed bug on 5.0.2.1589, 6.0.0.584 - got: "SQLSTATE = 08006 / Error reading data ...".
    Checked on 5.0.2.1592-2d11769, 6.0.0.585-6f17277 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    create exception exc_parallel_workers_required q'[Config parameter ParallelWorkers = @1 must be set to 2 or greater.]';
    create exception exc_udr_func_seems_not_working q'[UDR function 'sum_args' either not defined or is invalid]';
    create exception exc_not_enough_data_for_test q'[At least 2 PP must be allocated for data of 'TEST' table. Currently only @1 PP are used.]';

    -- create UDR
    create function sum_args (
        n1 integer,
        n2 integer,
        n3 integer
    ) returns integer
        external name 'udrcpp_example!sum_args'
        engine udr
    ;

    set term ^;
    execute block as
        declare n int;
    begin
        select rdb$config_value from rdb$config where rdb$config_name = 'ParallelWorkers' into n;
        if (n is null or n <= 1) then
            exception exc_parallel_workers_required using (n);
        --------------------------------------------
        select sum_args(1, 2, 3) from rdb$database into n;
        if (n is distinct from 6) then
            exception exc_udr_func_seems_not_working;

    end
    ^
    set term ;^
    commit;

    -- create table with dependency on UDR
    create table test (
        f1 int,
        f2 int,
        f3 int,
        f_sum computed by (sum_args(f1, f2, f3))
    );

    -- fill it with some data
    insert into test values (1, 1, 1);
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    insert into test select f1, f2, f3 from test;
    commit;

    set term ^;
    execute block as
        declare n int;
    begin
        -- make sure there are at least 2 pointer pages
        select count(*) from rdb$pages p join rdb$relations r on p.rdb$relation_id = r.rdb$relation_id
        where r.rdb$relation_name = upper('test') and p.rdb$page_type = 4
        into n;
        if (n < 2) then
            exception exc_not_enough_data_for_test using (n);

    end
    ^
    set term ;^
    commit;

    -- create index 
    create index test_idx_f1 on test(f1);

    -- THIS MUST BE DISPLAYED. CRASH WAS HERE BEFORE FIX:
    select 'Completed' as msg from rdb$database;
"""

act = isql_act('db', test_script, substitutions = [('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.2')
def test_1(act: Action):

    # DISABLED 17.01.2025 13:35, requested by dimitr:
    #if act.vars['server-arch'] != 'SuperServer':
    #    pytest.skip("Only SuperServer affected")

    act.expected_stdout = 'MSG Completed'
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

