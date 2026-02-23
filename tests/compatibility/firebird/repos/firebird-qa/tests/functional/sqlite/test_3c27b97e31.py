#coding:utf-8

"""
ID:          3c27b97e31
ISSUE:       https://www.sqlite.org/src/tktview/3c27b97e31
TITLE:       REAL rounding seems to depend on index presence
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create table t1(a real);
    insert into t1 values( 836627109860825358 );
    set count on;
    select * from t1 where a = cast(836627109860825358 as real); -- returns 1 row
    commit;

    create index i1 on t1(a);
    select * from t1 where a = cast(836627109860825358 as real); -- same query now returns 0 rows
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 8.3662712e+17
    Records affected: 1

    A 8.3662712e+17
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
