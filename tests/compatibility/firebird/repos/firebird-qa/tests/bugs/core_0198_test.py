#coding:utf-8

"""
ID:          issue-525
ISSUE:       525
TITLE:       Wrong order by in table join storedproc
DESCRIPTION:
JIRA:        CORE-198
FBTEST:      bugs.core_0198
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table my_table
    (
        k varchar(10) not null,
        d1 integer,
        d2 integer,
        v1 varchar(10),
        primary key (k)
    );

    set term ^;
    create or alter procedure select_me returns(
      data varchar(10)
    ) as
    begin
        data = 'one';
        suspend;
        data = 'two';
        suspend;
        data = 'three';
        suspend;
    end
    ^
    set term ;^
    commit;

    insert into my_table values ('one', 1, 99, 'zz');
    insert into my_table values ('two', 2, 98, 'yy');
    insert into my_table values ('three', 3, 97, 'xx');
    commit;

    set list on;

    select *
    from my_table t join select_me p on (t.k = p.data)
    order by t.d1
    ;
    commit;

    create index i1 on my_table(d1);
    commit;

    select *
    from my_table t join select_me p on (t.k = p.data)
    order by t.d1
    ;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    K                               one
    D1                              1
    D2                              99
    V1                              zz
    DATA                            one

    K                               two
    D1                              2
    D2                              98
    V1                              yy
    DATA                            two

    K                               three
    D1                              3
    D2                              97
    V1                              xx
    DATA                            three

    K                               one
    D1                              1
    D2                              99
    V1                              zz
    DATA                            one

    K                               two
    D1                              2
    D2                              98
    V1                              yy
    DATA                            two

    K                               three
    D1                              3
    D2                              97
    V1                              xx
    DATA                            three
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

