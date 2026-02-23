#coding:utf-8

"""
ID:          issue-5276
ISSUE:       5276
TITLE:       Non-privileged user can implicitly count records in a restricted table
DESCRIPTION:
JIRA:        CORE-4985
FBTEST:      bugs.core_4985
NOTES:
    [30.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
    
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='TMP$C4985', password='123')

test_script = """
    -- Checked on build  of 24.03.2016 4.0 Unstable.

    set wng off;
    create table test(id int);
    set count on;
    insert into test select 1 from rdb$types rows 7;
    commit;
    revoke all on all from TMP$C4985;
    commit;

    connect '$(DSN)' user 'TMP$C4985' password '123';

    set list on;
    select current_user as who_am_i from rdb$database;
    select count(*) from test;
    set count on;
    select 1 from test;

    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    expected_stdout = f"""
        Records affected: 7

        WHO_AM_I                        {tmp_user.name.upper()}
        Records affected: 1

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_NAME}
        -Effective user is {tmp_user.name.upper()}

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_NAME}
        -Effective user is {tmp_user.name.upper()}
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
