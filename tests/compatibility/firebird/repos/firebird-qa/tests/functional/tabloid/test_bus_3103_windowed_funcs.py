#coding:utf-8

"""
ID:          tabloid.bus-3103-windowed-funcs
TITLE:       Query for test MAX()OVER().
DESCRIPTION: 
FBTEST:      functional.tabloid.bus_3103_windowed_funcs
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-bus-3103.fbk')

test_script = """
    set list on;
    with
    dx as (
         select
            t.cid
            ,p.pid
            ,hash(src) + hash(tgt) sth
            ,datediff(minute from dts0 to dts1 + iif(t.dts1 < t.dts0, 1, 0)) dd
         from tmove t
            left join pmove p on t.id = p.tid
    )
    ,mx as (
        select
            dx.*,
            nullif( abs( max(dd)over() - max(dd)over(partition by cid, sth) ), 1) ns
         from dx
    )
    select
        (select name from clist c where c.id  = mx.cid) cnm,
        count(pid) cnt
    from   mx
    where  ns = 0
    group  by mx.cid
    having count(pid) > 0;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    CNM                             ba        
    CNT                             13
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
