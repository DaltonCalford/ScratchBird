#coding:utf-8

"""
ID:          issue-2066
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2066
TITLE:       CREATE OR ALTER VIEW statement
DESCRIPTION:
JIRA:        CORE-1640
FBTEST:      bugs.core_1640
NOTES:
    [05.10.2023] pzotov
    Removed SHOW VIEW command for check result because its output often changes.
"""

import pytest
from firebird.qa import db_factory, isql_act, Action

init_script = """
create table users (
    id integer,
    name varchar(20),
    passwd varchar(20)
);
commit;
insert into users(id, name, passwd) values(1,'john','12345');
commit;
"""

db = db_factory(init=init_script)

test_script = """
    set bail on;
    set list on;
    create or alter view v_users as
    select name from users;
    commit;

    alter view v_users (id, name, passwd ) as select id, name, passwd  from users;
    select * from v_users;
    commit;
    
    alter view v_users as select name from users;
    select * from v_users;
    commit;

    alter view v_users (id, name ) as select id, name from users;
    select * from v_users;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    NAME                            john
    PASSWD                          12345

    NAME                            john

    ID                              1
    NAME                            john
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
