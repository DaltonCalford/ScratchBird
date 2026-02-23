#coding:utf-8

"""
ID:          issue-6903
ISSUE:       6903
TITLE:       Unable to create ICU-based collation with locale keywords
DESCRIPTION:
FBTEST:      bugs.gh_6903
NOTES:
    [04.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- should PASS:
    create collation unicode_txt_01 for utf8 from unicode
      pad space
      case sensitive
      accent sensitive
      'LOCALE=de-u-co-phonebk-ka-shifted'
    ;

    -- should PASS:
    create collation unicode_txt_02 for utf8 from unicode
      'locale=en-u-kr-grek-latn-digit'
    ;

    -- should PASS:
    create collation unicode_txt_03 for utf8 from unicode
      pad space
      case sensitive
      accent sensitive
      'LOCALE=el@colCaseFirst=upper'
    ;

    -- should FAIL!
    -- See: https://github.com/FirebirdSQL/firebird/pull/6914
    -- Note by Adriano "I tried ... it worked, but should not"
    create collation unicode_bad_01 for utf8 from unicode
      'LOCALE=pt_BRx'
    ;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = HY000
    unsuccessful metadata update
    -CREATE COLLATION UNICODE_BAD_01 failed
    -Invalid collation attributes
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = HY000
    unsuccessful metadata update
    -CREATE COLLATION "PUBLIC"."UNICODE_BAD_01" failed
    -Invalid collation attributes
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
