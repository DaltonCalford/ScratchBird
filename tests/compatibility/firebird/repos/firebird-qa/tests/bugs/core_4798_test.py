#coding:utf-8

"""
ID:          issue-5096
ISSUE:       5096
TITLE:       Regression: MIN/MAX with a join ignores possible index navigation
DESCRIPTION:
JIRA:        CORE-4798
FBTEST:      bugs.core_4798
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='mon-stat-gathering-3_0.fbk')

test_script = """
    recreate table test(x int);
    commit;

    insert into test select rand()*100 from (select 1 i from rdb$types rows 50) a, (select 1 i from rdb$types rows 50) b;
    commit;
    create index test_x on test(x);
    commit;
    set term ^;
    execute block as
    begin
        -- ####################################
        -- S E T T I N G    T H R E S H O L D S
        -- ####################################
        rdb$set_context('USER_SESSION','NAT_READS_MAX_THRESHOLD', '0');
        rdb$set_context('USER_SESSION','IDX_READS_MAX_THRESHOLD', '20'); -- increased 27.07.2016
    end
    ^
    set term ;^

    alter sequence g_gather_stat restart with 0;
    execute procedure sp_truncate_stat;
    commit;

    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    set term ^;
    execute block as
        declare v_dummy int;
    begin
        select min(a.x) from test a join test b on a.x = b.x join test c on b.x = c.x into v_dummy;
    end
    ^
    set term ;^
    execute procedure sp_gather_stat;  ------- catch statistics AFTER measured statement(s)
    commit;

    set list on;
        -- 3.0.0.31374 (Beta 1):
        --    natural_reads                   2500
        --    indexed_reads                   1839808
        -- 3.0.0.31852:
        --    natural_reads                   0
        --    indexed_reads                   6
        -- 4.0.0.313:
        --    natural_reads                   0
        --    indexed_reads                   18
    select
         iif( natural_reads <= nat_max, 'acceptable, <= ' || nat_max, 'abnormal, ' || natural_reads || ' > ' || nat_max ) as natural_reads
        ,iif( indexed_reads <= idx_max, 'acceptable, <= ' || idx_max, 'abnormal, ' || indexed_reads || ' > ' || idx_max ) as indexed_reads
    from (
        select
             v.natural_reads
            ,v.indexed_reads
            ,cast(rdb$get_context('USER_SESSION','NAT_READS_MAX_THRESHOLD') as int) nat_max
            ,cast(rdb$get_context('USER_SESSION','IDX_READS_MAX_THRESHOLD') as int) idx_max
        from v_agg_stat v
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    NATURAL_READS                   acceptable, <= 0
    INDEXED_READS                   acceptable, <= 20
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

