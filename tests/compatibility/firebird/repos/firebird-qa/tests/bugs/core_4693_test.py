#coding:utf-8

"""
ID:          issue-5001
ISSUE:       5001
TITLE:       CTE Aliases
DESCRIPTION: 'Column unknown' when recursive CTE contain join with *alias* rather than table name
JIRA:        CORE-4693
FBTEST:      bugs.core_4693
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table gd_goodgroup (
        id integer,
        parent integer,
        name varchar(20)
    );
    commit;

    insert into gd_goodgroup(id, parent, name) values( 1, null, 'Big Brother');
    insert into gd_goodgroup(id, parent, name) values( 2,    1, 'Middle Manager #1');
    insert into gd_goodgroup(id, parent, name) values( 3,    1, 'Middle Manager #2');
    insert into gd_goodgroup(id, parent, name) values( 4,    2, 'Worker #1');
    insert into gd_goodgroup(id, parent, name) values( 5,    2, 'Worker #2');
    insert into gd_goodgroup(id, parent, name) values( 6,    3, 'Worker #3');
    commit;

    set list on;

    with recursive
    group_tree as (
        select id, parent, name, cast('' as varchar(255)) as indent
        from gd_goodgroup
        where parent is null

        union all

        select g.id, g.parent, g.name, h.indent || rpad('', 2)
        from gd_goodgroup g join group_tree h
        on g.parent = h.id
    )
    select gt.indent || gt.name as info
    from group_tree gt;
"""

act = isql_act('db', test_script)

expected_stdout = """
    INFO                            Big Brother
    INFO                              Middle Manager #1
    INFO                                Worker #1
    INFO                                Worker #2
    INFO                              Middle Manager #2
    INFO                                Worker #3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

