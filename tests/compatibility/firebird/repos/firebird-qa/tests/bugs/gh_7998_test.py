#coding:utf-8

"""
ID:          issue-7998
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7998
TITLE:       Crash during partial index checking if the condition raises a conversion error
NOTES:
    [10.02.2024] pzotov
    Confirmed bug on 6.0.0.250, ISQL issues errors and hangs (does not return control to OS):
        Statement failed, SQLSTATE = 22018
        Error during savepoint backout - transaction invalidated
        -conversion error from string "2"
        Statement failed, SQLSTATE = 25000
        transaction marked invalid and cannot be committed
    Checked on 6.0.0.257 -- all fine.

    [15.02.2024] pzotov
    Checked on 5.0.1.1340 -- all fine. Reduced min_version.

    [16.02.2024] pzotov
    Added [temporary] mark for SKIP this test when QA runs agains *fork* of standard FB because it hangs ('disabled_in_forks').
    This mark will be removed after separating QA runs (executing tests against standard FB snapshot on DEDICATED machine).

    NB-1. QA must use command like: 'pytest -m "not disabled_in_forks" ...' when check *fork* of standard FB.
    NB-2. Unfortunately, nowadays QA runs for standard FB and its fork are performed at the same host.
          Lagging problem exists with [back-]porting of some fixes/features into fork after implementation for the same FB-major version.
          This can cause the whole QA-job to be incompleted and missed report for one of even several days.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t0(c0 varchar(500), c1 int); 
    create unique index t0i0 on t0(c0 , c1 ) where (t0.c1 between false and true);
    insert into t0(c0, c1) values (1, 2);
"""

act = isql_act('db', test_script, substitutions = [('[ \t]+', ' ')])

expected_stdout = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "2"
"""

@pytest.mark.disabled_in_forks
@pytest.mark.version('>=5.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
