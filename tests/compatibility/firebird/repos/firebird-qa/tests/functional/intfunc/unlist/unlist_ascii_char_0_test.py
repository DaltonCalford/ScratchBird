#coding:utf-8

"""
ID:          issue-8418
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8418
TITLE:       UNLIST function. Check ability to use ascii_char(0) as delimiter.
DESCRIPTION:
    On 6.0.0.725-a2b05f4-x64 usage of ascii_char(0) as separator causes 100% CPU load and FB service could not be stopped
    See: https://github.com/FirebirdSQL/firebird/pull/8418#issuecomment-2792358627
    Fixed 19.04.2025 20:36:
    https://github.com/FirebirdSQL/firebird/commit/33ad7e632ae073223f808c8fdc83673d6d04e454
NOTES:
    [23.04.2025] pzotov
    Checked on 6.0.0.744
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    set list on;
    set count on; select * from unlist('1', ascii_char(0)) as u(x);
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

expected_stdout = f"""
    X 1
    Records affected: 1
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
