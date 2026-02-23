#coding:utf-8

"""
ID:          gtcs.parser-comments-in-sql
TITLE:       Check for problems with comments (single-line and multi-line)
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_19.script

  bug #781610 problems with one line comments (--)
FBTEST:      functional.gtcs.parser_comments_in_sql
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;

    create table test (n integer);
    insert into test values (1);

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


    -- Added 18.03.2020
    --#################

    -- single-line comment '*/
    select * from test;

    -- single-line comment --*/
    select * from test;

    /* * / / * q'{
       BEGIN multi-line comment-1
       '*/
    select * from test;


    /* '' BEGIN multi-line comment-2
       '* / / *  */
    select * from test;


"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    1
    1
    1
    1
    1
    1
    1
    1
    1
    1
    1
    1
    1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
