#coding:utf-8

"""
ID:          issue-8086
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8086
TITLE:       IN predicate with string-type elements is evaluated wrongly against a numeric field
DESCRIPTION:
NOTES:
    [06.05.2024] pzotov
    Confirmed bug on 6.0.0.336
    Checked on 6.0.0.344, 5.0.1.1394
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;

    recreate table test (
        id int primary key
    );
    commit;

    insert into test (id) values (1);
    insert into test (id) values (2);
    insert into test (id) values (3);
    insert into test (id) values (11);
    insert into test (id) values (12);
    insert into test (id) values (13);

    set count on;

    -- this worked fine:
    select 1 as "case-1", t.* from rdb$database r left join test t on t.id in ('1','12') order by t.id;
    select 2 as "case-2", t.* from rdb$database r left join test t on t.id in (2,12) order by t.id;
    select 3 as "case-3", t.* from rdb$database r left join test t on t.id in ('02','12') order by t.id;

    -- this worked wrong before fix:
    select 4 as "case-4", t.* from rdb$database r left join test t on t.id in ('2','12') order by t.id;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    case-1    1
    ID        1
    case-1    1
    ID       12
    Records affected: 2

    case-2    2
    ID        2
    case-2    2
    ID       12
    Records affected: 2

    case-3    3
    ID        2
    case-3    3
    ID       12
    Records affected: 2

    case-4   4
    ID       2
    case-4   4
    ID      12
    Records affected: 2
"""

@pytest.mark.version('>=5.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
