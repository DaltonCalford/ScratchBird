#coding:utf-8

"""
ID:          1b266395d6
ISSUE:       https://www.sqlite.org/src/tktview/1b266395d6
TITLE:       INSERT OR REPLACE with a foreign key constraint leads to assertion fault
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (id integer primary key, parentid integer references test(id) on delete cascade, c1 char(10));
    update or insert into test(id, parentid, c1) values (1, null, 'a') matching (id);
    update or insert into test(id, parentid, c1) values (2, 1, 'a-2-1');
    update or insert into test(id, parentid, c1) values (3, 2, 'a-3-2');
    update or insert into test(id, parentid, c1) values (4, 3, 'a-4-3');
    update or insert into test(id, parentid, c1) values (2, 3, 'a-2-3');

    set count on;
    select * from test;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 1
    PARENTID <null>
    C1 a

    ID 2
    PARENTID 3
    C1 a-2-3

    ID 3
    PARENTID 2
    C1 a-3-2

    ID 4
    PARENTID 3
    C1 a-4-3

    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
