#coding:utf-8

"""
ID:          issue-6088
ISSUE:       6088
TITLE:       ALTER CURRENT USER fails with "no permission for <...> TABLE PLG$SRP" if current user: 1) has NO admin role and 2) wants to modify his own TAGS list
DESCRIPTION:
  Code of this test must to be changed after ticket will be fixed!
  See line with 'grant admin role' -- it must me COMMENTED.
  Also, min_version should be set to 3.0.x rather than 4.0.0

  Currently we check only ability to change TAGS list using 'ALTER CURRENT USER' statement.
  See also test for CORE-3365, but it checks only 'old' attributes which existed before FB 3.0.
JIRA:        CORE-5827
FBTEST:      bugs.core_5827
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp$c5827', plugin='Srp', do_not_create=True)

test_script = """
    set bail on;
    set list on;

    create user tmp$c5827
        password 'UseSrp'
        firstname 'Mary'
    -- NB: no error will be raised if we UNCOMMENT this line; IMO this is bug, see ticket issue;
    -- TODO: comment must be here, put it later when this ticket issue will be fixed.
    -- >>> commented 25.05.2021 >>> grant admin role <<< all OK.
    using plugin Srp
        tags (
             key1 = 'val111'
            ,key2 = 'val222'
            ,key3 = 'val333'
        )
    ;
    commit;

    connect '$(DSN)' user tmp$c5827 password 'UseSrp';

    --- passed w/o error:
    alter current user
        set password 'FooSrp' firstname 'Scott' lastname 'Tiger'
        using plugin Srp
    ;
    commit;

    -- DOES raise error if current user has no admin role:
    alter current user
        using plugin Srp
        tags (
             Foo = 'Bar'
            ,key1 = 'val11'
            ,Rio = '1565'
            ,drop key3
            ,drop key2
        )
    ;
    commit;
"""

act_1 = isql_act('db', test_script)

@pytest.mark.version('>=3.0.4')
def test_1(act_1: Action, tmp_user: User):
    act_1.execute()

