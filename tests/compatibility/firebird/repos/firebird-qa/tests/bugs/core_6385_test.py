#coding:utf-8

"""
ID:          issue-6624
ISSUE:       6624
TITLE:       Wrong line and column information after IF statement
DESCRIPTION:
  DO NOT make indentation or excessive empty lines in the code that is executed by ISQL.
JIRA:        CORE-6385
FBTEST:      bugs.core_6385
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block
    as
        declare n integer;
    begin
        if (1 = 1) then
            n = 1;
        n = n / 0;
    end^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|(At\\s+block\\s+line)).)*$', ''),
                                                 ('[ \t]+', ' ')])

expected_stdout = """
    Statement failed, SQLSTATE = 22012
    -At block line: 7, col: 9
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
