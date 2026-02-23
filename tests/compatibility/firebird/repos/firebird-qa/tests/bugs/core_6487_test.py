#coding:utf-8

"""
ID:          issue-6717
ISSUE:       6717
TITLE:       FETCH ABSOLUTE and RELATIVE beyond bounds of cursor should always position immediately before-first or after-last
DESCRIPTION:
JIRA:        CORE-6487
FBTEST:      bugs.core_6487
NOTES:
    [03.07.2025] pzotov
    Suppressed name of cursor - it has no matter in this test.
    Checked on 6.0.0.892; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    set term ^;
    execute block returns(id int, rc int) as
        declare c scroll cursor for
        (
            select  1 id from rdb$database union all
            select  2 id from rdb$database union all
            select  3 id from rdb$database
        )
        ;
    begin
        open c;
        fetch absolute 9223372036854775807 from c;

        fetch relative -(9223372036854775807-2) from c;
        id = c.id;
        rc = row_count;
        suspend;

        close c;
    end
    ^

    execute block returns(id int, rc int) as
        declare c scroll cursor for
        (
            select  1 id from rdb$database union all
            select  2 id from rdb$database union all
            select  3 id from rdb$database
        )
        ;
    begin
        open c;
        fetch absolute -9223372036854775808 from c;

        fetch relative (9223372036854775806) from c;
        id = c.id;
        rc = row_count;
        suspend;

        close c;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('-At block line:.*', '-At block line'), ('Cursor \\S+ is not positioned', 'Cursor is not positioned')])

expected_stdout = """
    Statement failed, SQLSTATE = HY109
    Cursor is not positioned in a valid record
    -At block line

    Statement failed, SQLSTATE = HY109
    Cursor is not positioned in a valid record
    -At block line
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
