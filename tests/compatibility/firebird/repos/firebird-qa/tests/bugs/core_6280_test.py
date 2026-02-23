#coding:utf-8

"""
ID:          issue-6522
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6522
TITLE:       MERGE statement loses parameters in WHEN (NOT) MATCHED clause that will never be matched, crashes server in some situations
DESCRIPTION:
NOTES:
    [14.12.2021] pcisar
    It's impossible to reimplement it in the same way with new driver.
    PROBLEM:
    Original test used two parameter values where 3 parameters are expected, but
    new driver does not even allow that as it checks number of values with number of
    parameters - returned by iMessageMetadata.get_count().
    ALSO, as new driver uses OO API, it does not use SQLDA structures at all.

    [29.12.2023] pzotov
    Problem can be reproduced if we run MERGE using ISQL utility with 'set sqlda_display on'.
    Example was provided by Mark:
    https://github.com/FirebirdSQL/firebird/issues/6522#issuecomment-826246877

    Confirmed crash on 3.0.6.33283 (date of build: 15.04.2020).
    Checked on 3.0.6.33285 (16.04.2020) -- all fine.
    Checked on 6.0.0.195, 5.0.0.1305, 4.0.5.3049.
JIRA:        CORE-6280
FBTEST:      bugs.core_6280
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t(i int not null primary key, j int);
"""

db = db_factory(init=init_script)

test_script = """
    set sqlda_display on;
    merge into t using (select 1 x from rdb$database) on 1 = 1
    when matched then update set j = ?
    when matched and i = ? then delete
    when not matched then insert (i, j) values (1, ?);

"""

act = isql_act('db', test_script, substitutions=[('^((?!sqltype:|SQLSTATE|[Ee]rror|SQLDA).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    03: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4

    Statement failed, SQLSTATE = 07002
    Dynamic SQL Error
    -SQLDA error
    -No SQLDA for input values provided
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
