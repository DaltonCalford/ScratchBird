#coding:utf-8

"""
ID:          issue-6889
ISSUE:       6889
TITLE:       error no permision occurred while ALTER USER SET TAGS on snapshot build WI-V3.0.8.33482
DESCRIPTION:
FBTEST:      bugs.gh_6889
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter user tmp$gh_6889 password '123' using plugin Srp;
    commit;

    connect '$(DSN)' user tmp$gh_6889 password '123';
    alter current user set tags ( active2 = 'TRUE' );
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$gh_6889 using plugin Srp;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
