#coding:utf-8

"""
ID:          752e1646fc
ISSUE:       https://www.sqlite.org/src/tktview/752e1646fc
TITLE:       Wrong result if DISTINCT used on subquery which uses ORDER BY.
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (f01 varchar(1) primary key, f02 integer not null);

    insert into test (f01, f02) values('b', 1);
    insert into test (f01, f02) values('a', 2);
    insert into test (f01, f02) values('c', 2);

    set count on;
    select distinct f02
    from (
        select f01, f02 from test order by f01, f02 rows 1
    ) as t;

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    F02 2
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
