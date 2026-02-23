#coding:utf-8

"""
ID:          issue-2372
ISSUE:       2372
TITLE:       SIMILAR TO character classes are incorrectly recognized
DESCRIPTION:
JIRA:        CORE-1935
FBTEST:      bugs.core_1935
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- two subsequent classes in double braces, concatenated together:
    select 11 as r from rdb$database where '1a' similar to '[[:DIGIT:]][[:ALPHA:]]'; -- output: 11

    -- comparison with SINGLE class of ONE character length: either digit or whitespace or alpha:
    select 12 as r from rdb$database where '1a' similar to '[[:DIGIT:][:WHITESPACE:][:ALPHA:]]'; -- no output, no error

    -- comparison with character '2' followed by either digit or whitespace or alpha (should produce non-empty result):
    select 21 as r from rdb$database where '2a' similar to '2[[:DIGIT:][:WHITESPACE:][:ALPHA:]]'; -- output: 21

    -- comparison with SINGLE class of ONE character length: digit either alnum either alpha
    select 22 as r from rdb$database where '2a' similar to '[[:DIGIT:][:ALNUM:][:ALPHA:]]'; -- no output, no error

    -- comparison with TWO classes: 1st is result of concatenation alnum and whitespace, 2ns is alnum:
    select 31 as r from rdb$database where '3a' similar to '[[:ALNUM:][:WHITESPACE:]][[:ALNUM:]]'; -- 31

    -- comparison with TWO classes: 1st alnum, 2nd is result of concatenation whitespace and digit:
    select 32 as r from rdb$database where '32' similar to '[[:ALNUM:]][[:WHITESPACE:][:DIGIT:]]'; -- 32

    select 41 as r from rdb$database where '4a' SIMILAR TO '[%[:DIGIT:][:ALNUM:]]%';

    select 42 as r from rdb$database where '4b' SIMILAR TO '[[:DIGIT:][:ALNUM:]]'; -- no output, no error

    select 51 as r from rdb$database where '5a' SIMILAR TO '%[[:DIGIT:][:ALNUM:]%]'; -- 51

    select 52 as r from rdb$database where '5a' similar to '[%[:DIGIT:][:ALPHA:]][[:ALNUM:]%]'; -- 52
"""

act = isql_act('db', test_script)

expected_stdout = """
    R                               11
    R                               21
    R                               31
    R                               32
    R                               41
    R                               51
    R                               52
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

