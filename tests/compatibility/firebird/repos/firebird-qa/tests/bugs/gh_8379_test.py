#coding:utf-8

"""
ID:          issue-8379
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8379
TITLE:       Incorrect cardinality estimation for retrievals with multiple compound indices having common set of fields
DESCRIPTION:
    
NOTES:
    [15.01.2025] pzotov

    1. This ticket can NOT be checked on FB 5.x: there are no differences neither in explained plans nor in trace statistics
       for snapshots before and after fix. Only cardinality estimation can be checked but this feature avaliable only in FB-6
       via rdb$sql.explain() package (see doc/sql.extensions/README.sql_package.md).
    2. Cardinality change can be seen only for second example provided in the ticket
       ("Test #2: three conditions, two of them are mapped ... the third one is not mapped to an index")
       // see also letter from dimitr 14.01.2025 15:06

    3. Query :
           select
               plan_line as p_line
               ,cardinality
               ,access_path
           from rdb$sql.explain(q'#
               select count(*) as cnt_1
               from t
               where f1 = 99 and f2 = 99 and f3 = 99
           #')
       - must return rows with *different* cardinality for plan lines with "Filter" and "Table ... Access By ID":
           P_LINE    CARDINALITY        ACCESS_PATH                 |
           1             <null>          Select expression          |
           2           1                 -> Aggregate               |
           3           1.78675004326506  -> Filter                  | <<< THIS VALUE MUST BE MUCH LESS THAN ONE FOR P_LINE = 4 ('card_afte')
           4          17.8675004326506   -> Table ... Access By ID  | <<< THIS IS VALUE BEFORE 'FILTER' IS APPLIED ('card_befo')
     Before fix this was not so: cardinality value in line with "Filter" was the same as in line for "Table ... Access By ID":
     Thanks to dimitr for suggestions.

     Confirmed bug on 6.0.0.576.
     Checked on 6.0.0.577 - all OK.
"""

import pytest
from firebird.qa import *

#############
N_ROWS = 5000
OK_MSG = 'Cardinality estimation: EXPECTED.'
#############

init_script = f"""
    recreate view v_check_card as select 1 x from rdb$database;
    recreate table test (id int primary key, f1 int, f2 int, f3 int);

    set term ^;
    execute block as
        declare n int = {N_ROWS};
        declare i int  = 0;
    begin
        while (i < n) do
        begin
             insert into test(id, f1, f2, f3) values(:i, mod(:i, 100), mod(:i, 200), mod(:i, 300));
             i = i + 1;
        end
    end
    ^
    set term ;^
    commit;

    create index it1 on test(f1, f2);
    create index it2 on test(f1, f3);
    commit;

    recreate view v_check_card as
    with
    a as (
        select
            plan_line
            ,record_source_id
            ,parent_record_source_id
            ,level
            ,cardinality
            ,record_length
            ,key_length
            ,access_path
        from rdb$sql.explain(q'#
            select count(*) as cnt_1
            from test
            where f1 = 99 and f2 = 99 and f3 = 99
        #')
    )
    ,b as (
        select
             a.plan_line
            ,a.cardinality as card_afte
            ,lead(a.cardinality)over(order by a.plan_line) card_befo
            ,a.access_path
        from a
    )
    select
        iif(  b.card_afte / nullif(b.card_befo,0) < 1
             ,'{OK_MSG}'
             ,'Cardinality estimation INCORRECT, HAS NOT REDUCED: '
               || 'card_befo = ' || b.card_befo
               || ', card_afte = ' || b.card_afte
               || ', card_afte / card_befo = ' || (b.card_afte / nullif(b.card_befo,0))
           ) as msg
    from b
    where b.access_path containing 'filter';
"""

db = db_factory(init=init_script)

test_script = """
    set heading off;
    select * from v_check_card;
"""

act = isql_act('db', test_script)

expected_stdout = f"""
    {OK_MSG}
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
