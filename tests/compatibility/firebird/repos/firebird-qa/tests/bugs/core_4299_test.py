#coding:utf-8

"""
ID:          issue-4622
ISSUE:       4622
TITLE:       Inappropriate self-reference of column when using "with check option" with extract(...)
DESCRIPTION:
JIRA:        CORE-4299
FBTEST:      bugs.core_4299
"""

import pytest
from firebird.qa import *

init_script = """
    -- Source description found here:
    -- http://stackoverflow.com/questions/20505769/inappropriate-self-reference-of-column-when-using-with-check-option-in-fireb
    create or alter view foo as select current_date dt from rdb$database;
    commit;

    recreate table bar(dt date);
    commit;
    insert into bar(dt) values ('28.03.2011');
    commit;

    create or alter view foo as
    select * from bar
    where extract(year from bar.dt) = '2011'
    with check option
    ;
"""

db = db_factory(init=init_script)

test_script = """
  set list on;
  select * from foo;
"""

act = isql_act('db', test_script)

expected_stdout = """
  DT                              2011-03-28
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

