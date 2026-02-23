#coding:utf-8

"""
ID:          issue-7025
ISSUE:       7025
TITLE:       Results of negation must be the same for each datatype (smallint / int /bigint / int128) when argument is least possible value for this type
DESCRIPTION: Confirmed 'sqltype: 496 LONG' for -(-2147483648). Before fix was: '580 INT64'.
FBTEST:      bugs.gh_7025
NOTES:
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set sqlda_display on;
    select -(-32768) as neg_of_2p15 from rdb$database;
    select -(-2147483648) as neg_of_2p31 from rdb$database;
    select -(-9223372036854775808) as neg_of_2p63 from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!sqltype:|NEG_OF_2P|SQLSTATE|overflow).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
    :  name: CONSTANT  alias: NEG_OF_2P15

    NEG_OF_2P15                     32768

    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
    :  name: CONSTANT  alias: NEG_OF_2P31

    Statement failed, SQLSTATE = 22003
    Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.

    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
    :  name: CONSTANT  alias: NEG_OF_2P63

    Statement failed, SQLSTATE = 22003
    Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
