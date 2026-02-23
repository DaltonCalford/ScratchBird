#coding:utf-8

"""
ID:          issue-3967
ISSUE:       3967
TITLE:       Plan returned for query with recursive CTE return wrong count of parenthesis
DESCRIPTION:
JIRA:        CORE-3614
FBTEST:      bugs.core_3614
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test_tree(
        id integer not null,
        id_header integer,
        constraint pk_test_tree__id primary key (id)
    );

    create index ixa_test_tree__id_header on test_tree (id_header);
    commit;

    insert into test_tree values ('1', null);
    insert into test_tree values ('2', null);
    insert into test_tree values ('3', null);
    insert into test_tree values ('4', '1');
    insert into test_tree values ('5', '4');
    insert into test_tree values ('6', '2');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set planonly;
    with recursive
    r_tree as
    (
        select tt.id as a, cast(tt.id as varchar(100)) as asum
        from test_tree tt
        where tt.id_header is null

        union all

        select tt.id as a, rt.asum || '_' || tt.id
        from test_tree tt join r_tree rt on rt.a = tt.id_header
    )
    select * from r_tree rt2  join test_tree tt2 on tt2.id=rt2.a ;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
    for line in act.stdout.splitlines():
        if 'PLAN' in line and (line.count('(') - line.count(')') != 0):
            pytest.fail(f"Difference in opening vs close parenthesis: {line.count('(') - line.count(')')}")


