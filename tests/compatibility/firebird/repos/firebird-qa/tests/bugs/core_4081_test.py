#coding:utf-8

"""
ID:          issue-4409
ISSUE:       4409
TITLE:       Regression: Built-in functions and subselect no longer supported in "update or insert" value list
DESCRIPTION:
JIRA:        CORE-4081
FBTEST:      bugs.core_4081
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test (uid varchar(64));
    commit;

    update or insert into test (uid) values ( uuid_to_char(gen_uuid()) )
    matching (uid);

    update or insert into test (uid)
    values ( (select uuid_to_char(gen_uuid()) from rdb$database) )
    matching (uid);
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
