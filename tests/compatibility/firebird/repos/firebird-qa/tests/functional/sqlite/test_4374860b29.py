#coding:utf-8

"""
ID:          4374860b29
ISSUE:       https://www.sqlite.org/src/tktview/4374860b29
TITLE:       Segfault
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
    create table v0(v3 int, v1 varchar(10) unique);
    create table v5(v6 int unique, v7 int unique);
    create view v8(v9) as select coalesce(v3, v1) from v0 where v1 in('med box');

    set count on;
    select * from v8 cross join v5 where 0 > v7 and v9 > 0 or v6 = 's%';
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
