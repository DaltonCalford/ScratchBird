#coding:utf-8

"""
ID:          fd1bda016d
ISSUE:       https://www.sqlite.org/src/tktview/fd1bda016d
TITLE:       Assertion in the query containing subquery in select section and exists()
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
    recreate table t0(v1 varchar(10));
    insert into t0 values(2);
    insert into t0 values(3);
    
    set count on;
    select 0 in (select v1 from t0) as chk
    from t0
    where v1 = 2 or exists(select v1 from t0 rows 0);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CHK <false>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
