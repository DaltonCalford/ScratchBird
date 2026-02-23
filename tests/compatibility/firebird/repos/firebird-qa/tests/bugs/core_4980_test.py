#coding:utf-8

"""
ID:          issue-5271
ISSUE:       5271
TITLE:       Operator REVOKE can modify rights granted to system tables at DB creation time
DESCRIPTION:
  We create here NON-privileged user and revoke any right from him. Also create trivial table TEST.
  Then try to connect with as user and query non-system table TEST and system tables.
  Query to table TEST should be denied, but queries to RDB-tables should run OK and display their data.
JIRA:        CORE-4980
FBTEST:      bugs.core_4980
NOTES:
    [30.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
    Separated expected output for FB major versions prior/since 6.x.
    
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
import locale
import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp_c4980', password='123')

act = isql_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):

    test_script = f"""
        set wng off;

        recreate table test(id int);
        commit;
        insert into test values(1);
        commit;

        connect '{act.db.dsn}' user {tmp_user.name.upper()} password '{tmp_user.password}';

        -- All subsequent statements (being issued by TMP_C4980) failed on 3.0.0.32134 and runs OK on build 32136:
        set list on;

        select current_user as who_am_i from rdb$database;
        select current_user as who_am_i, r.rdb$character_set_name from rdb$database r;
        select current_user as who_am_i, r.rdb$relation_name from rdb$relations r order by rdb$relation_id rows 1;
        select current_user as who_am_i, t.id from test t; -- this should ALWAYS fail because this is non-system table.
        commit;
    """

    expected_stdout_3x = f"""
        WHO_AM_I                        {tmp_user.name.upper()}
        WHO_AM_I                        {tmp_user.name.upper()}
        RDB$CHARACTER_SET_NAME          NONE
        WHO_AM_I                        {tmp_user.name.upper()}
        RDB$RELATION_NAME               RDB$PAGES
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE TEST
    """

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    expected_stdout_5x = f"""
        WHO_AM_I                        {tmp_user.name.upper()}
        WHO_AM_I                        {tmp_user.name.upper()}
        RDB$CHARACTER_SET_NAME          NONE
        WHO_AM_I                        {tmp_user.name.upper()}
        RDB$RELATION_NAME               RDB$PAGES
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_NAME}
        -Effective user is {tmp_user.name.upper()}
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x
    act.isql(switches = ['-q'], input = test_script, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
