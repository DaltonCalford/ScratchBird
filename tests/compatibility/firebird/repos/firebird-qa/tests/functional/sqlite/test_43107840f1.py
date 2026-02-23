#coding:utf-8

"""
ID:          43107840f1
ISSUE:       https://www.sqlite.org/src/tktview/43107840f1
TITLE:       Assertion fault on UPDATE
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
    create table t1(a integer primary key, b char(10));
    insert into t1(a,b) values(10,'abc');
    commit;
    alter table t1 add c char(16)  character set octets;
    create index t1c on t1(c);
    insert into t1(a,b,c) values(5,'def','ghi');
    set count on;
    update t1 set c = gen_uuid() where c is null;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
