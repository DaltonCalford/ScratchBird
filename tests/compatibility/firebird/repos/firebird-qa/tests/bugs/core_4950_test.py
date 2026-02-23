#coding:utf-8

"""
ID:          issue-5241
ISSUE:       5241
TITLE:       Statistics for any system index can not be updated/recalculated
DESCRIPTION:
JIRA:        CORE-4950
FBTEST:      bugs.core_4950
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table stat(idx_name varchar(31), stat_init double precision, stat_curr double precision);
    recreate table test(id int primary key, pid int references test(id), x int, y int, unique(x,y));
    create or alter view v_stat as
    select
      iif( ri.rdb$index_name starting with 'RDB$PRIMARY',
           'RDB$PRIMARY',
           iif( ri.rdb$index_name starting with 'RDB$FOREIGN',
                'RDB$FOREIGN',
                'RDB$UNIQUE'
              )
         ) as idx_name
     ,ri.rdb$statistics idx_stat
    from rdb$indices ri
    where ri.rdb$relation_name = upper('test');
    commit;

    insert into test(id, pid, x, y) values( 0, null, null, null);

    set term ^;
    execute block as
        declare i int = 1;
        declare n int = 999;
    begin
        while (i < n) do
        begin
            insert into test(id, pid, x, y) values( :i, :i / 2, iif( mod(:i,2)=0, :i/2, null ), iif( mod(:i,2)=0, null, :i/2 ));
            i = i + 1;
        end
    end
    ^
    set term ;^
    commit;

    insert into stat(idx_name, stat_init) select idx_name, idx_stat from v_stat;
    commit;

    set list on;

    set term ^;
    execute block as
    begin
        for
            select trim(ri.rdb$index_name) idx_name
            from rdb$indices ri
            where ri.rdb$relation_name = upper('test')
            as cursor c
        do
            execute statement 'set statistics index ' || c.idx_name;
    end
    ^
    set term ;^
    commit;

    merge into stat t
    using(select idx_name, idx_stat from v_stat) s
    on t.idx_name = s.idx_name
    when matched then update set t.stat_curr = s.idx_stat
    when not matched then insert(idx_name, stat_curr) values( s.idx_name, s.idx_stat);

    commit;

    select idx_name, abs( sign( stat_curr - stat_init ) ) stat_diff
    from stat
    stat order by idx_name;
"""

act = isql_act('db', test_script)

expected_stdout = """
    IDX_NAME                        RDB$FOREIGN
    STAT_DIFF                       1

    IDX_NAME                        RDB$PRIMARY
    STAT_DIFF                       1

    IDX_NAME                        RDB$UNIQUE
    STAT_DIFF                       1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

