#coding:utf-8

"""
ID:          issue-3860
ISSUE:       3860
TITLE:       DROP VIEW ignores the existing non-column dependencies
DESCRIPTION:
JIRA:        CORE-3502
FBTEST:      bugs.core_3502
NOTES:
    [27.06.2025] pzotov
    Added subst to suppress output: it is enough to display error message w/o concrete coulmn name for this test.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter view v (id) as select rdb$relation_id from rdb$database;
    commit;

    set term ^;
    create or alter procedure p as
      declare id int;
    begin
      select id from v rows 1 into id;
    end^
    set term ;^
    commit;

    execute procedure p;
    commit;

    drop view v;
"""

act = isql_act('db', test_script, substitutions = [('(-)?COLUMN .*', 'COLUMN *')])

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    COLUMN *
    -there are 1 dependencies
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
