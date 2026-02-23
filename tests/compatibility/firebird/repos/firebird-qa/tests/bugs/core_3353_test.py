#coding:utf-8

"""
ID:          issue-3719
ISSUE:       3719
TITLE:       Predicate 'blob_field LIKE ?' describes the parameter as VARCHAR(30) rather than as BLOB
DESCRIPTION:
JIRA:        CORE-3353
FBTEST:      bugs.core_3353
NOTES:
    Code was splitted for 3.x and 4.x+ because:
    1) output in 3.0 will contain values of sqltype with ZERO in bit_0, so it will be: 520 instead of previous 521.
       (see also: core_4156.fbt)
    2) we have to explicitly specify connection charset for FB 3.x otherwise 'UNICODE_FSS' will be issued in SQLDA

    [10.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.

    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set planonly;
    select rdb$procedure_source from rdb$procedures where rdb$procedure_source like ?;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')])

# version: 3.0

expected_stdout_1 = """
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 UTF8
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 UTF8
"""


@pytest.mark.version('>=3.0,<4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    # NB: we have to specify charset for FB 3.x otherwise 'UNICODE_FSS' will be issued in SQLDA:
    act.execute(charset='utf8',combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

######################################################################################################

# version: 4.0

expected_out_5x = """
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 UTF8
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 UTF8
"""

expected_out_6x = """
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 SYSTEM.UTF8
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 SYSTEM.UTF8
"""

@pytest.mark.version('>=4.0')
def test_2(act: Action):
    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

