#coding:utf-8

"""
ID:          3be1295b26
ISSUE:       https://www.sqlite.org/src/tktview/3be1295b26
TITLE:       Inconsistent behavior of a partial unique index on a boolean expression.
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (c0 varchar(10), c1 boolean);
    create unique index index_0 on test computed by (c1 = false);
    --create unique index index_0 on test computed by (c1 in (true, false));
    create index  index_1 on test computed by(c0 || false) where c1;

    insert into test(c0, c1) values('a',true);
    insert into test(c0, c1) values('a',false);
    set count on;
    select r.*, c0 || false as v1, c1 = false as v2 from test r;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 a
    C1 <true>
    V1 aFALSE
    V2 <false>

    C0 a
    C1 <false>
    V1 aFALSE
    V2 <true>

    Records affected: 2
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
