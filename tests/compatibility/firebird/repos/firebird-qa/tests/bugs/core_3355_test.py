#coding:utf-8

"""
ID:          issue-3721
ISSUE:       3721
TITLE:       Wrong comparsion of DATE and TIMESTAMP if index is used
DESCRIPTION:
JIRA:        CORE-3355
FBTEST:      bugs.core_3355
"""

import pytest
from firebird.qa import *

init_script = """
    create table tdate (id integer not null primary key, val date);
    create index tdateix1 on tdate (val);
    commit;
    insert into tdate values (0, '1997-12-31');
    insert into tdate values (1, '1998-01-01');
    insert into tdate values (2, '1998-01-02');
    insert into tdate values (3, '1998-01-03');
    insert into tdate values (4, '1998-01-04');
    insert into tdate values (5, '1998-01-05');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select count(*) as cnt_01 from tdate where val >= timestamp'1998-01-04 12:00:00.0000';
    select count(*) as cnt_02 from tdate where val < timestamp'1998-01-04 12:00:00.0000';
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CNT_01 1
    CNT_02 5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

