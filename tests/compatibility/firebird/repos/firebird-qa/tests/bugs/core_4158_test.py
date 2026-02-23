#coding:utf-8

"""
ID:          issue-4485
ISSUE:       4485
TITLE:       Regression: LIKE with escape does not work
DESCRIPTION:
JIRA:        CORE-4158
FBTEST:      bugs.core_4158
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table tab1 (
      id int constraint pk_tab1 primary key,
      val varchar(30)
    );

    insert into tab1 (id, val) values (1, 'abcdef');
    insert into tab1 (id, val) values (2, 'abc_ef');
    insert into tab1 (id, val) values (3, 'abc%ef');
    insert into tab1 (id, val) values (4, 'abc&%ef');
    insert into tab1 (id, val) values (5, 'abc&_ef');
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select id, val from tab1 where val like 'abc&%ef' escape '&';
    select id, val from tab1 where val like 'abc&_ef' escape '&';
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              3
    VAL                             abc%ef
    ID                              2
    VAL                             abc_ef
"""

@pytest.mark.version('>=2.0.7')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

