#coding:utf-8

"""
ID:          issue-4537
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/4537
TITLE:       Dropping FK on GTT crashes server
DESCRIPTION:
JIRA:        CORE-4212
FBTEST:      bugs.core_4212
NOTES:
    [05.10.2023] pzotov
    Confirmed crash on 3.0.0.30566 Alpha1.
    Removed SHOW command. It is enough for this test just to try to insert record in the 'T2' table after dropping FK.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'win1251')

test_script = """
    set list on;
    create global temporary table t1 (text_id varchar(8) not null primary key);
    create global temporary table t2 (text_id varchar(8));
    alter table t2 add constraint t2_fk foreign key (text_id) references t1 (text_id);
    commit;

    connect '$(DSN)';

    alter table t2 drop constraint t2_fk;
    commit;
    insert into t2(text_id) values('qwerty');
    select * from t2;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+', ''), ] )

expected_stdout = """
    TEXT_ID qwerty
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
