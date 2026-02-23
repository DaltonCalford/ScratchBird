#coding:utf-8

"""
ID:          bfbf38e5e9
ISSUE:       https://www.sqlite.org/src/tktview/bfbf38e5e9
TITLE:       Segfault on a nested join
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t1(id1 int, value1 int);
    recreate table t2 (value2 int);

    insert into t1 values(4469,2);
    insert into t1 values(4469,1);
    insert into t2 values(1);

    --select
    --    (select sum(iif( value2 = value1, 1, 0)) from t2),

    set count on;
    select (select sum(iif( t2.value2 <> x.value1, 1, 0)) from t2)
    from (
        select max(value1) as value1
        from t1
        group by id1
    ) x;
    set count off;
    commit;
    ---------------------------------------------------------------

    recreate table aaa (
      aaa_id       integer primary key
    );
    recreate table rrr (
      rrr_id      integer     primary key,
      rrr_date    integer     not null,
      rrr_aaa     integer
    );
    recreate table ttt (
      ttt_id      integer primary key,
      target_aaa  integer not null,
      source_aaa  integer not null
    );

    insert into aaa (aaa_id) values (2);

    insert into ttt (ttt_id, target_aaa, source_aaa) values (4469, 2, 2);
    insert into ttt (ttt_id, target_aaa, source_aaa) values (4476, 2, 1);

    insert into rrr (rrr_id, rrr_date, rrr_aaa) values (0, 0, null);
    insert into rrr (rrr_id, rrr_date, rrr_aaa) values (2, 4312, 2);

    set count on;
    select i.aaa_id,
      (select sum(case when (t.source_aaa = i.aaa_id) then 1 else 0 end)
         from ttt t
      ) as segfault
    from (
        select max(curr.rrr_aaa) as aaa_id, max(r.rrr_date) as rrr_date
        from rrr curr
        -- you also can comment out the next line
        -- it causes segfault to happen after one row is outputted
        inner join aaa a on (curr.rrr_aaa = aaa_id)
        left join rrr r on (r.rrr_id <> 0 and r.rrr_date < curr.rrr_date)
        group by curr.rrr_id
        having max(r.rrr_date) is null
    ) i;
    set count off;
    commit;
    ---------------------------------

    recreate table t1 (
        id1 integer primary key,
        value1 integer
    );

    recreate table t2 (
        id2 integer primary key,
        value2 integer
    );

    insert into t1(id1, value1) values(4469,2);
    insert into t1(id1, value1) values(4476,1);
    insert into t2(id2, value2) values(0,1);
    insert into t2(id2, value2) values(2,2);

    set count on;
    select
       (select sum(iif(value2=xyz,1,0)) from t2)
    from (
        select max(curr.value1) as xyz
        from t1 as curr
        left join t1 as other on curr.id1 = other.id1
        group by curr.id1
    );
    set count off;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    SUM 1
    Records affected: 1
    
    AAA_ID 2
    SEGFAULT 1
    Records affected: 1
    
    SUM 1
    SUM 1
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
