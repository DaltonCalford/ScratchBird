#coding:utf-8

"""
ID:          issue-5718
ISSUE:       5718
TITLE:       EXECUTE STATEMENT <e> when <e> starts with '--' issues -Unexpected ... column <NNN>,
  value <NNN> is invalid and can change randomly
DESCRIPTION:
  We run EB that is show in the ticket three times, with redirection STDOUT and STDERR to separate files.
  Then we open file of STDERR and parse it: search for lines which contain "-Unexpected end of command" text.
  Extract nineth word from this and add it to Python structure of type = SET.
  Finally, we check that this set:
  1) all columns are positive integers;
  2) contains only one element (i.e. all columns have the same value).
JIRA:        CORE-5447
FBTEST:      bugs.core_5447
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_script = """
    set term ^;
    execute block as
    begin
        execute statement '-- table ''test'' has no trigger, DROP TRIGGER is skipped.';
    end
    ^
    execute block as
    begin
        execute statement '-- table ''test'' has no trigger, DROP TRIGGER is skipped.';
    end
    ^
    execute block as
    begin
        execute statement '-- table ''test'' has no trigger, DROP TRIGGER is skipped.';
    end
    ^
    set term ;^
"""

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):
    act.expected_stderr = "We expect errors"
    act.isql(switches=[], input=test_script)
    col_set = set()
    for line in act.stderr.splitlines():
        if '-Unexpected end of command' in line:
            # -Unexpected end of command - line 0, column -45949567
            #    0         1  2    3     4   5  6    7       8
            col_number = line.split()[8]
            assert col_number.isdigit() and int(col_number) > 0, "column is ZERO, NEGATIVE or NaN"
            col_set.add(col_number)
    assert len(col_set) == 1, "columns differ or empty set()"
