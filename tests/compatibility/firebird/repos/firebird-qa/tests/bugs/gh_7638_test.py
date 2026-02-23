#coding:utf-8

"""
ID:          issue-7638
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7638
TITLE:       OVERRIDING USER VALUE should be allowed for GENERATED ALWAYS AS IDENTITY
DESCRIPTION:
NOTES:
    [29.06.2023] pzotov
    Checked on 5.0.0.1093 (intermediate build).
    Checked on 4.0.3.2966 (after backport 08-JUL-2023).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table IDENTITY_ALWAYS (
      ID bigint generated always as identity constraint pk_identity_always primary key,
      VAL varchar(10)
    );
    commit;

    insert into IDENTITY_ALWAYS (ID, VAL) overriding user value values (-9223372036854775808, 'B1') returning ID, VAL;
    insert into IDENTITY_ALWAYS (ID, VAL) overriding user value values (null, 'B2') returning ID, VAL;
    insert into IDENTITY_ALWAYS (ID, VAL) overriding user value values (cast(9223372036854775808 as int128), 'B3') returning ID, VAL;
"""


act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    VAL                             B1
    ID                              2
    VAL                             B2
    ID                              3
    VAL                             B3
"""

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
