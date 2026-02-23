#coding:utf-8

"""
ID:          c88f3036a2
ISSUE:       https://www.sqlite.org/src/tktview/c88f3036a2
TITLE:       ALTER TABLE DROP <column> may corrupt data
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(id integer primary key, f01 int, f02 int);
    insert into t1 select i, 123, 456 from (select row_number()over() as i from rdb$types, rdb$types rows 50000);
    commit;
    alter table t1 drop f01;

    select count(*), f02 from t1 group by f02;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 50000
    F02 456
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
