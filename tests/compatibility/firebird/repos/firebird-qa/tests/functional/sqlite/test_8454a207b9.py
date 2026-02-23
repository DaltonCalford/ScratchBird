#coding:utf-8

"""
ID:          8454a207b9
ISSUE:       https://www.sqlite.org/src/tktview/8454a207b9
TITLE:       ALTER TABLE ADD COLUMN with negative DEFAULT value
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    create table t1(a int);
    alter table t1
        add b varchar(50) default '-3e-308'
       ,add c computed by(cast(b as double precision))
    ;
    insert into t1 default values;
    insert into t1(a) values(1);
    set count on;
    select * from t1 order by a;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A <null>
    B -3e-308
    C -3.000000000000000e-308

    A 1
    B -3e-308
    C -3.000000000000000e-308
    
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
