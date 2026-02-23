#coding:utf-8

"""
ID:          aa43786930
ISSUE:       https://www.sqlite.org/src/tktview/aa43786930
TITLE:       Assertion
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table v0(v1 int unique, v2 int unique);
    create view v4 as select * from v0 where v2 < 10 or v1 < 7 order by v2;

    insert into v0(v2) values(0);

    set count on;
    select '1' as res from v0 left join v4 on null is null;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    RES 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
