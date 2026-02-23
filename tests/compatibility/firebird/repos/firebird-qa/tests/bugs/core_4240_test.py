#coding:utf-8

"""
ID:          issue-4564
ISSUE:       4564
TITLE:       Regression: recursive query in SQL query returns incorrect results if more than one branch bypass
DESCRIPTION:
JIRA:        CORE-4240
FBTEST:      bugs.core_4240
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    with recursive h
    as (select 1 as code_horse,
               2 as code_father,
               3 as code_mother
        from rdb$database
        union all
        select 2 as code_horse,
               4 as code_father,
               5 as code_mother
        from rdb$database
        union all
        select 3 as code_horse,
               4 as code_father,
               5 as code_mother
        from rdb$database
        union all
        select 4 as code_horse,
               null as code_father,
               null as code_mother
        from rdb$database
        union all
        select 5 as code_horse,
               null as code_father,
               null as code_mother
        from rdb$database),
    r
    as (select h.code_horse as code_horse,
               h.code_father as code_father,
               h.code_mother as code_mother,
               cast('' as varchar(10)) as mark,
               0 as depth
        from h
        where h.code_horse = 1
        union all
        select h.code_horse as code_horse,
               h.code_father as code_father,
               h.code_mother as code_mother,
               'f' || r.mark as mark,
               r.depth + 1 as depth
        from r
        join h on r.code_father = h.code_horse
        where r.depth < 5
        union all
        select h.code_horse as code_horse,
               h.code_father as code_father,
               h.code_mother as code_mother,
               'm' || r.mark as mark,
               r.depth + 1 as depth
        from r
        join h on r.code_mother = h.code_horse
        where r.depth < 5)
    select *
    from r
    order by 1,2,3,4,5
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('=', '')])

expected_stdout = """
  CODE_HORSE  CODE_FATHER  CODE_MOTHER MARK              DEPTH
           1            2            3                       0
           2            4            5 f                     1
           3            4            5 m                     1
           4       <null>       <null> ff                    2
           4       <null>       <null> fm                    2
           5       <null>       <null> mf                    2
           5       <null>       <null> mm                    2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
