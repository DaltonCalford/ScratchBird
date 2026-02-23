#coding:utf-8

"""
ID:          80369eddd5
ISSUE:       https://www.sqlite.org/src/tktview/80369eddd5
TITLE:       Incorrect case in the LIKE operator when comparing unicode characters belonging to "Other Letter" category
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Description of used characters:
        https://www.compart.com/en/unicode/U+304D
        https://www.compart.com/en/unicode/U+306D
    See also:
        https://www.ssec.wisc.edu/~tomw/java/unicode.html (full list of unicode scopes and characters)
    Function unicode_char() exists in FB 5.x+
    Checked on 6.0.0.1232, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    select 'き' LIKE 'ね' as v1 from rdb$database;
    -- 0x304d = Hiragana letter ki
    -- 0x306d = Hiragana letter Ne
    select unicode_char(0x304d) like unicode_char(0x306d) as v2 from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V1 <false>
    V2 <false>
"""

@pytest.mark.intl
@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
