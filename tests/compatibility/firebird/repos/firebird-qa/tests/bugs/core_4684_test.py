#coding:utf-8

"""
ID:          issue-4992
ISSUE:       4992
TITLE:       Error while preparing a complex query ("Too many Contexts of Relation/Procedure/Views. Maximum allowed is 256")
DESCRIPTION:
JIRA:        CORE-4684
FBTEST:      bugs.core_4684
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table tdetl(id int primary key, pid int);
    recreate table tmain(id int primary key, x int, constraint tmain_x_gtz check(x>0));
    commit;
    alter table tdetl add constraint tdetl_fk foreign key(pid) references tmain(id) on delete cascade;
    commit;
    set term ^;
    create or alter trigger tdetl_bi for tdetl active before insert as begin end ^
    create or alter trigger tdetl_ai for tdetl active after insert as begin end ^
    create or alter trigger tmain_bi for tmain inactive before insert as begin end ^
    create or alter trigger tmain_ai for tmain active after insert as begin end ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set heading off;
    with recursive
    c as (
        select
             rc.rdb$relation_name child_tab
            ,rc.rdb$constraint_name child_fk
            ,ru.rdb$const_name_uq parent_uk
            ,rp.rdb$relation_name parent_tab
        from rdb$relation_constraints rc
        join rdb$ref_constraints ru on
             rc.rdb$constraint_name = ru.rdb$constraint_name
             and rc.rdb$constraint_type = 'FOREIGN KEY'
        join rdb$relation_constraints rp
             on ru.rdb$const_name_uq = rp.rdb$constraint_name
        where rc.rdb$relation_name <> rp.rdb$relation_name
    )
    ,d as(
        select
            0 i
            ,child_tab
            ,child_fk
            ,parent_uk
            ,parent_tab
        from c c0
        where not exists( select * from c cx where cx.parent_tab= c0.child_tab )

        union all

        select
            d.i+1
            ,c.child_tab
            ,c.child_fk
            ,c.parent_uk
            ,c.parent_tab
        from d
        join c on d.parent_tab = c.child_tab
    )
    ,e as(
        select
            i
            ,child_tab
            ,child_fk
            ,parent_uk
            ,parent_tab
            ,(select max(i) from d) as mi
        from d
    )
    ,f as(
        select distinct
            0 i
            ,child_tab
        from e where i=0

        UNION DISTINCT

        select
            1
            ,child_tab
        from (select child_tab from e where i>0 order by i)

        UNION DISTINCT

        select
            2
            ,parent_tab
        from e
        where i=mi
    )
    ,t as(
        select
            rt.rdb$trigger_name trg_name -- f.child_tab, rt.rdb$trigger_name, rt.rdb$trigger_type
        from f
        join rdb$triggers rt on f.child_tab = rt.rdb$relation_name
        where rt.rdb$system_flag=0 and rt.rdb$trigger_inactive=0
    )
    select 'alter trigger '||trim(trg_name)||' inactive' sql_expr
    from t
    union all
    select 'delete from '||trim(child_tab)
    from f
    union all
    select 'alter trigger '||trim(trg_name)||' active'
    from t;
"""

act = isql_act('db', test_script)

expected_stdout = """
alter trigger TDETL_BI inactive
alter trigger TDETL_AI inactive
alter trigger TMAIN_AI inactive
delete from TDETL
delete from TMAIN
alter trigger TDETL_BI active
alter trigger TDETL_AI active
alter trigger TMAIN_AI active
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

