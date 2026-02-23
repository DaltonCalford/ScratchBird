#coding:utf-8

"""
ID:          issue-3367
ISSUE:       3367
TITLE:       The new 2.5 feature to alter COMPUTED columns doesn't handle dependencies well
DESCRIPTION:
JIRA:        CORE-2985
FBTEST:      bugs.core_2985
NOTES:
    [27.06.2025] pzotov
    Replaced 'SHOW' command with query to 't_dependant'.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t_source (id numeric, f1 varchar(20));
    create table t_dependant(id1 numeric, ff computed( (select s.f1 from t_source s where s.id = id1) ) );
    commit;
    insert into t_dependant(id1) values(1);
    commit;
    alter table t_dependant alter ff computed(cast(null as varchar(20)));
    drop table t_source;
    commit;
    set count on;
    select * from t_dependant;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID1                             1
    FF                              <null>
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

