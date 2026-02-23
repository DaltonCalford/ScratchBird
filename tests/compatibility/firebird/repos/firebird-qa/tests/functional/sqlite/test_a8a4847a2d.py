#coding:utf-8

"""
ID:          a8a4847a2d
ISSUE:       https://www.sqlite.org/src/tktview/a8a4847a2d
TITLE:       Trigger inserts duplicate value in UNIQUE column
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
    create table t0(c0 int unique using index t0_unq);
    set term ^;
    create trigger tr0 after delete on t0 as
    begin
        insert into t0 values(0); 
    end
    ^
    set term ;^
    update or insert into t0(c0) values(0) matching(c0);
    update or insert into t0(c0) values(0) matching(c0);
    commit;
    alter index t0_unq active;
    set count on;
    select * from t0;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 0
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
