#coding:utf-8

"""
ID:          isql_one_line_comments_01
TITLE:       bug #781610 problems with one line comments (--)
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_19.script
NOTES:
    [12.03.2025] pzotov
    Checked on 6.0.0.660; 5.0.3.1630; 4.0.6.3190; 3.0.13.33798.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test (n integer);
    insert into test values (1);

    set echo;

    -- I am a comment

    /* BEGIN */
    -- I am a comment
    select * from test;
    /* END */

    /* BEGIN */
    -- comment with unclosed 'quoted string
    select * from test;
    /* END */

    /* BEGIN */
    -- comment with unclosed "quoted string
    select * from test;
    /* END */

    /* BEGIN */
    -- I am a comment;
    select * from test;
    /* END */

    /* BEGIN with unclosed "quoted */
    -- I am a comment;
    select * from test;
    /* END */

    select * /*
    comment
    */
    from test;

    select * 
    /* comment */
    from test;

    select * 
    -- comment
    from test;

    /*
    Comment
    */ select * from test;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    -- I am a comment
    /* BEGIN */
    -- I am a comment
    select * from test;
    N
    1
    /* END */
    /* BEGIN */
    -- comment with unclosed 'quoted string
    select * from test;
    N
    1
    /* END */
    /* BEGIN */
    -- comment with unclosed "quoted string
    select * from test;
    N
    1
    /* END */
    /* BEGIN */
    -- I am a comment;
    select * from test;
    N
    1
    /* END */
    /* BEGIN with unclosed "quoted */
    -- I am a comment;
    select * from test;
    N
    1
    /* END */
    select * /*
    comment
    */
    from test;
    N
    1
    select *
    /* comment */
    from test;
    N
    1
    select *
    -- comment
    from test;
    N
    1
    /*
    Comment
    */ select * from test;
    N
    1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
