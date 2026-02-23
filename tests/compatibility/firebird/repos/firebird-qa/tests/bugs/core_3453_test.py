#coding:utf-8

"""
ID:          issue-3814
ISSUE:       3814
TITLE:       Added not null timestamp col with default causes error on select of old null records
DESCRIPTION:
JIRA:        CORE-3453
FBTEST:      bugs.core_3453
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    create table "Temp" ("Dummy" int);
    commit;
    insert into "Temp" ("Dummy") values (1);
    commit;
    alter table "Temp" add "New" timestamp default '0001-01-01' not null;
    commit;
    set list on;
    select * from "Temp";
"""

act = isql_act('db', test_script)

expected_stdout = """
    Dummy                           1
    New                             0001-01-01 00:00:00.0000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

