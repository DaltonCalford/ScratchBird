#coding:utf-8

"""
ID:          issue-4984
ISSUE:       4984
TITLE:       Conditions like WHERE <field> = <cursor>.<field> do not use existing index
DESCRIPTION:
JIRA:        CORE-4675
FBTEST:      bugs.core_4675
"""

import pytest
from firebird.qa import *

init_script = """
    commit;
    set transaction no wait;
    recreate table t1(id int primary key using index pk_t1_id, x int, y int);
    commit;

    set transaction no wait;
    recreate table t2(id int primary key using index pk_t2_id, x int, y int);
    commit;

    insert into t1
    select r, mod(r, 10), (r/100)*100
    from (
      select row_number()over() r
      from rdb$types a, (select 1 i from rdb$types rows 40) b
      rows 1000
    );
    commit;
    create index t1_x on t1(x);
    commit;

    insert into t2 select * from t1;
    commit;

    create index t2_x on t2(x);
    commit;
"""

db = db_factory(from_backup='mon-stat-gathering-3_0.fbk', init=init_script)

test_script = """
    execute procedure sp_truncate_stat;
    commit;
    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    set transaction no wait;
    set term ^;
    execute block as
      declare a_x int;
      declare v_id int;
      declare v_x int;
      declare v_y int;
      declare c_upd1 cursor for (select id, x, y from t1 where x = :a_x);
      declare c_upd2 cursor for (select id, x, y from t2 where x = :a_x);
    begin
      a_x = 5;

      open c_upd1;
      while (1=1) do begin
        fetch c_upd1 into v_id, v_x, v_y;
        if (row_count = 0) then leave;
        update t1 v set y = c_upd1.x, x = c_upd1.y
        where v.id = :v_id; ----------------- ::: key is specified by VARIABLE which has value from FETCH statement
      end
      close c_upd1;

      open c_upd2;
      while (1=1) do begin
        fetch c_upd2; -- into v_id, v_x, v_y;
        if (row_count = 0) then leave;
        update t2 v set y = c_upd2.x, x = c_upd2.y
        where v.id = c_upd2.id; ------------- ::: key is specified by CURSOR field using "cursor name + dot + field" syntax
      end
      close c_upd2;

    end
    ^ set term ;^
    rollback;

    execute procedure sp_gather_stat;  ------- catch statistics AFTER measured statement(s)
    commit;

    set list on;
    set width table_name 31;

    -- Show results (differences of saved monitoring counters):
    -- ========================================================

    select v.table_name, v.natural_reads, v.indexed_reads
    from v_agg_stat_tabs v
    where upper(v.table_name) in ( upper('T1'), upper('T2') );

    -- Output for WI-T6.3.0.31374 (Beta 1 official):
    -- TABLE_NAME                      T1
    -- NATURAL_READS                   0
    -- INDEXED_READS                   200
    --
    -- TABLE_NAME                      T2
    -- NATURAL_READS                   100000 <<<< this is bad and has been fixed in CORE_4675
    -- INDEXED_READS                   100

"""

act = isql_act('db', test_script)

expected_stdout = """
    TABLE_NAME                      T1
    NATURAL_READS                   0
    INDEXED_READS                   200

    TABLE_NAME                      T2
    NATURAL_READS                   0
    INDEXED_READS                   200
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

