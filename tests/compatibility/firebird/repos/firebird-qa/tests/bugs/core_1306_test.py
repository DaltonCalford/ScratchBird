#coding:utf-8

"""
ID:          issue-1726
ISSUE:       1726
TITLE:       Indices not used for views
DESCRIPTION:
JIRA:        CORE-1306
FBTEST:      bugs.core_1306
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table "TABLE" (id integer not null primary key);
    commit;
    insert into "TABLE" (id) values (1);
    insert into "TABLE" (id) values (2);
    insert into "TABLE" (id) values (3);
    commit;
    create view "VIEW" as select * from "TABLE";
    commit;
    set plan on;
    select * from "TABLE" where id = 1
    union all
    select * from "VIEW" where id = 1 ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (TABLE INDEX (RDB$PRIMARY1), VIEW TABLE INDEX (RDB$PRIMARY1))
    ID 1
    ID 1
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TABLE" INDEX ("PUBLIC"."RDB$PRIMARY1"), "PUBLIC"."VIEW" "PUBLIC"."TABLE" INDEX ("PUBLIC"."RDB$PRIMARY1"))
    ID 1
    ID 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

