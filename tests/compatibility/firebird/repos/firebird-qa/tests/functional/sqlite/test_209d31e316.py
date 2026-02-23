#coding:utf-8

"""
ID:          209d31e316
ISSUE:       https://www.sqlite.org/src/tktview/209d31e316
TITLE:       Assertion fault when deleting a table out from under a SELECT
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
    create table t1(id integer primary key,b char);
    insert into t1(id,b) values(1,'a');
    insert into t1(id,b) values(2,'b');
    insert into t1(id,b) values(3,'c');
    commit;
    set term ^;
    create function fn_killer(a_id int) returns int as
    begin
        execute statement ('delete from t1 where id = ?') (a_id) with autonomous transaction;
        return row_count;
    end
    ^
    set term ;^
    commit;
    set transaction read committed;
    set count on;
    select id, fn_killer(id), b, (select count(*) from t1 x where x.id<>t1.id) as cnt_remain from t1 order by id desc;
    commit;
    select * from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 3
    FN_KILLER 0
    B c
    CNT_REMAIN 2

    ID 2
    FN_KILLER 0
    B b
    CNT_REMAIN 1

    ID 1
    FN_KILLER 0
    B a
    CNT_REMAIN 0
    Records affected: 3

    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
