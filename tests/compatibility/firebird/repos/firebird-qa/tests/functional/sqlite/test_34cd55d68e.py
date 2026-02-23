#coding:utf-8

"""
ID:          34cd55d68e
ISSUE:       https://www.sqlite.org/src/tktview/34cd55d68e
TITLE:       Database corruption following INSERT with a TRIGGER that does an affinity change
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
    create table t1(ii int);
    create table t2(tt char(10) primary key, ss char(10));

    set term ^;
    create trigger t1_ai after insert on t1 as
    begin
      insert into t2(tt) values(new.ii);
    end
    ^
    create trigger t2_ai after insert on t2 as
    begin
      update t2 set ss = 4;
    end
    ^
    set term ;^
    commit;
    insert into t1(ii) values('1');
    set count on;
    select * from t2;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    TT 1
    SS 4
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
