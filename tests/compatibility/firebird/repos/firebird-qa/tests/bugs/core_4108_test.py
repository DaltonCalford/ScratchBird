#coding:utf-8

"""
ID:          issue-4436
ISSUE:       4436
TITLE:       Regression: Server crashes when executing sql query "delete from mytable order by id desc rows 2"
DESCRIPTION:
JIRA:        CORE-4108
FBTEST:      bugs.core_4108
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table mytable (
        id integer not null primary key,
        name varchar(30)
    );

    insert into mytable(id, name)
    select 1, 'a' from rdb$database
    union all
    select 2, 'b' from rdb$database
    union all
    select 3, 'c' from rdb$database;

    delete from mytable order by id desc rows 2;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
