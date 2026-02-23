#coding:utf-8

"""
ID:          issue-6419
ISSUE:       6419
TITLE:       "No current record for fetch operation" with queries with aggregated subselect
DESCRIPTION:
JIRA:        CORE-6171
FBTEST:      bugs.core_6171
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    recreate table tmain( s varchar(10) );
    recreate table tdetl( s varchar(10), u int );
    commit;

    insert into tmain(s) values('foo');
    insert into tmain(s) values('bar');
    insert into tmain(s) values('rio');
    insert into tmain(s) values('boo');
    insert into tmain(s) values('');
    commit;

    insert into tdetl(s, u) values('foo', 100);
    insert into tdetl(s, u) values('bar', 200);
    insert into tdetl(s, u) values('rio', 300);
    insert into tdetl(s, u) values('boo', 400);
    commit;

    create index tmain_s on tmain(s);
    commit;

    set heading off;
    select r.s
    from tmain r
    where
        r.s = ''
        or (
               select sum(d.u)
               from tdetl d
               where d.s = r.s
           ) > 0
    ;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    foo
    bar
    rio
    boo
"""

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
