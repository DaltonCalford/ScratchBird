#coding:utf-8

"""
ID:          issue-2338
ISSUE:       2338
TITLE:       Dropping and adding a domain constraint in the same transaction leaves incorrect dependencies
DESCRIPTION:
JIRA:        CORE-1907
FBTEST:      bugs.core_1907
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

init_script = """create table t1 (n integer);
create table t2 (n integer);

create domain d1 integer check (value = (select n from t1));
"""

db = db_factory(init=init_script)

test_script = """set autoddl off;

alter domain d1 drop constraint;
alter domain d1 add constraint check (value = (select n from t2));
commit;

drop table t1; -- cannot drop - there are dependencies
commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = ''
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
