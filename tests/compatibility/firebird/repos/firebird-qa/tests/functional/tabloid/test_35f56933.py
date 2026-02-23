#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/35f56933306d9d486a5c66da8f85b4be214860d9
TITLE:       Fixed cardinality mistake for invariant booleans
DESCRIPTION:
NOTES:
    [18.11.2024] pzotov
    1. No ticket has been created for this test.
    2. Currently one may see cardinality to nodes of explained plan only in the output of rdb$sql.explain()
    3. Before fix, last two nodes of query like 'select ... from <table> where 1=1' had different values
       of cardinality. If cardinality for last node was is <C> then for node <N-1> it was wrongly evaluated
       as power(C,2). After fix these two values must be the same.
    
    Thanks to dimitr for the explaiantion on implementing the test.

    Confirmed problem on 6.0.0.520.
    Checked on 6.0.0.532 -- all fine.

    12.07.2025 DEFERRED REGRESION, SENT Q TO ADRIANO & DIMITR

"""

import pytest
from firebird.qa import *

test_sql = f"""
    set list on;
    recreate sequence g;
    recreate table test(id int primary key);
    insert into test select gen_id(g,1) from rdb$types,rdb$types rows 1000;
    commit;
    select
        t.access_path
        ,iif(    count(distinct t.cardinality)over() = 1
                ,'EXPECTED: THE SAME.'
                ,'UNEXPECTED: min = ' || min(t.cardinality)over() || ', max=' || max(t.cardinality)over()
            ) as cardinality_values
    from (
        select
             p.plan_line               
            ,p.record_source_id        
            ,p.parent_record_source_id 
            ,p.level                   
            ,p.cardinality
            ,cast(p.access_path as varchar(8190)) as access_path
            ,max(p.plan_line)over() - p.plan_line as mx
        from rdb$sql.explain('select count(*) from test where 1=1') as p
    ) t
    where t.mx in (0,1)
    ;                                                     
"""

db = db_factory()
act = isql_act('db', test_sql, substitutions=[('[ \t]+', ' ')])


@pytest.mark.version('>=6')
def test_1(act: Action):
   
    TEST_TABLE_NAME = '"TEST"' if act.is_version('<6') else '"PUBLIC"."TEST"'
    act.expected_stdout = f"""
        ACCESS_PATH -> Filter (preliminary)
        CARDINALITY_VALUES EXPECTED: THE SAME.

        ACCESS_PATH -> Table {TEST_TABLE_NAME} Full Scan
        CARDINALITY_VALUES EXPECTED: THE SAME.
    """
    act.execute(combine_output = True)

    assert act.clean_stdout == act.clean_expected_stdout
