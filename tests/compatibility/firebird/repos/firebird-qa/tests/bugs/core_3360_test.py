#coding:utf-8

"""
ID:          issue-3726
ISSUE:       3726
TITLE:       update ... returning ... raises -551 (no perm to update) for a column present
  only in the returning clause
DESCRIPTION:
JIRA:        CORE-3360
FBTEST:      bugs.core_3360
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set wng off;

    recreate table test(id int, readonly_x int, readonly_y int, writeable_column int);
    commit;

    insert into test(id, readonly_x, readonly_y, writeable_column) values(1, 100, 200, 300);
    commit;

    grant select on test to tmp$c3360;
    grant update (writeable_column) on test to tmp$c3360;
    commit;

    connect '$(DSN)' user 'TMP$C3360' password '123';

    update test set writeable_column = readonly_x - readonly_y where id = 1 returning writeable_column;
    commit;
"""

act = isql_act('db', test_script)

test_user = user_factory('db', name='tmp$c3360', password='123')

expected_stdout = """
    WRITEABLE_COLUMN                -100
"""

@pytest.mark.version('>=3')
def test_1(act: Action, test_user: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

