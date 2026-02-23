#coding:utf-8

"""
ID:          issue-5464
ISSUE:       5464
TITLE:       Regression: line/column numbering may be twisted if alias.name syntax is used
DESCRIPTION:
  NB: it's very _poor_ idea to compare line and column values from text of failed statement
  and some concrete values because they depend on position of statement whithin text ('sqltxt')
  which we going to execute by ISQL.
  Thus it was decided to check only that at final point we will have error log with only ONE
  unique pair of values {line, column} - no matter which exactly values are stored there.
  For this purpose we run script, filter its log (which contains text like: -At line NN, column MM)
  and parse (split) these lines on tokens. We extract tokens with number line and column and add
  each pair to the dictionary (Python; Map in java). Name of variable for this dict. = 'pairs'.

  Key point: length of this dictionary should be 1.
JIRA:        CORE-5183
FBTEST:      bugs.core_5183
"""

import pytest
import re
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_script = """
set term ^;
execute block
returns (id int)
as
begin
  select y
    from rdb$database x where z = 0
    into id;
  suspend;
end^

execute block
returns (id int)
as
begin
  select x.y
    from rdb$database x where z = 0
    into id;
  suspend;
end^
"""

@pytest.mark.version('>=2.5.6')
def test_1(act: Action):
    pattern = re.compile("-At line[\\s]+[0-9]+[\\s]*,[\\s]*column[\\s]+[0-9]+")
    act.expected_stderr = "We expect errors"
    act.isql(switches=[], input=test_script)
    # stderr Output
    # -At line 6, column 35
    # -At line 9, column 5
    #  ^   ^   ^    ^    ^
    #  |   |   |    |    |
    #  1   2   3    4    5 <<< indices for tokens
    pairs = {}
    for line in act.stderr.splitlines():
        if pattern.match(line):
            tokens = re.split('\\W+', line)
            pairs[tokens[3]] = tokens[5]
    print(pairs)
    assert len(pairs) == 1
