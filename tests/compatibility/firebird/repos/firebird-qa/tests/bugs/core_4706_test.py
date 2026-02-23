#coding:utf-8

"""
ID:          issue-5014
ISSUE:       5014
TITLE:       ISQL pads blob columns wrongly when the column alias has more than 17 characters
DESCRIPTION:
JIRA:        CORE-4706
FBTEST:      bugs.core_4706
NOTES:
    [04.07.2025] pzotov
    Added check for column with maximal possible size = 63 characters.
    Added case when column headers are in non-ascii form.
    Blobs ID output is suppressed.
    Increased min_version to 4.0
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set blob all;
    -- select cast('a' as blob) a, 1, cast('a' as blob) x2345678901234567890, 2 from rdb$database;
    select
        cast('a' as blob) a
        ,1
        ,cast('b' as blob) x2345678901234567890
        ,2
        ,cast('c' as blob) x23456789012345678901234567890123456789012345678901234567890123
        ,3
    from rdb$database;

    select
        cast('a' as blob) "α"
        ,1
        ,cast('b' as blob) "έαισορροπίαθαείναικά"
        ,2
        ,cast('c' as blob) "έαισορροπίαθαείναικάτωαπότομηδέαισορροπίαθαείναικάτωαπότομηδέα"
        ,3
    from rdb$database;
"""

substitutions = [ ('\\d:\\d', 'x:x'), ('={3,}', '') ]
act = isql_act('db', substitutions = substitutions)

expected_stdout = """
    A     CONSTANT X2345678901234567890     CONSTANT X23456789012345678901234567890123456789012345678901234567890123     CONSTANT
    x:x            1                  x:x            2                                                             x:x            3
    A:
    a
    X2345678901234567890:
    b
    X23456789012345678901234567890123456789012345678901234567890123:
    c
    α     CONSTANT έαισορροπίαθαείναικά     CONSTANT έαισορροπίαθαείναικάτωαπότομηδέαισορροπίαθαείναικάτωαπότομηδέα     CONSTANT
    x:x            1                  x:x            2                                                            x:x            3
    α:
    a
    έαισορροπίαθαείναικά:
    b
    έαισορροπίαθαείναικάτωαπότομηδέαισορροπίαθαείναικάτωαπότομηδέα:
    c
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], charset = 'utf8', input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
