#coding:utf-8

"""
ID:          issue-5557
ISSUE:       5557
TITLE:       Granting access rights to view is broken
DESCRIPTION:
JIRA:        CORE-5279
FBTEST:      bugs.core_5279
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='tmp$c5279', password='123')

test_script = """
    recreate table test (id integer);
    recreate table test1 (id integer);
    commit;
    create or alter view v_test as
    select *
    from test
    where id in (select id from test1);
    commit;
    grant select on v_test to public;
    grant select on test1 to view v_test;
    commit;
    insert into test(id) values(1);
    insert into test(id) values(2);
    insert into test(id) values(3);

    insert into test1(id) values(3);
    insert into test1(id) values(4);
    insert into test1(id) values(5);
    commit;

    connect '$(DSN)' user tmp$c5279 password '123';
    set count on;
    set list on;
    select current_user as who_am_i, v.* from v_test v;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    WHO_AM_I                        TMP$C5279
    ID                              3
    Records affected: 1
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action, tmp_user: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

