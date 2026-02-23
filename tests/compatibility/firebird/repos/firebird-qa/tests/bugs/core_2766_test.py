#coding:utf-8

"""
ID:          issue-3158
ISSUE:       3158
TITLE:       Error "page 0 is of wrong type (expected 6, found 1)" is thrown while accessing a non-corrupted table
DESCRIPTION:
JIRA:        CORE-2766
FBTEST:      bugs.core_2766
"""

import pytest
from firebird.qa import *

init_script = """
    create table t_master (id int not null, name varchar(64));
    alter table t_master add constraint PK_master primary key (id);

    create table t_detail (id_master int not null, name varchar(64));
    alter table t_detail add constraint FK_detail foreign key (id_master) references t_master (id);

    commit;

    insert into t_master values (1, '1');
    insert into t_detail values (1, 'a');
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """
    insert into t_master values (3, '2');
    delete from t_master where id = 3;
    commit;

    drop table t_detail;
    commit;

    delete from t_master;

    set list on;
    select count(*) from t_master;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

