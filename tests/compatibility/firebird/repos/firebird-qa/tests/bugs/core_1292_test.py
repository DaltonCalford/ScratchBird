#coding:utf-8

"""
ID:          issue-1713
ISSUE:       1713
TITLE:       Can't create table using long username and UTF8 as attachment charset
DESCRIPTION:
JIRA:        CORE-1292
FBTEST:      bugs.core_1292
NOTES:
    [23.08.2024] pzotov
    1. Removed LIST() from initial query because it displays tokens in unpredictable order.
       This can cause fail if we change OptimizeForFirstRows = true config parameter.
    2. Found oddities when try to use non-ascii user name and substitute it using f-notation:
       at least REVOKE and GRANT commands reflect this user name in the trace as encoded
       in cp1251 instead of utf8. This causes:
       335544321 : arithmetic exception, numeric overflow, or string truncation
       335544565 : Cannot transliterate character between character sets
       To be investigated further.
"""
import locale
import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

act = python_act('db', substitutions = [ ('[ \t]+', ' '), ])
tmp_user = user_factory('db', name='Nebuchadnezzar_The_Babylon_Lord', password='123', plugin = 'Srp')
#tmp_user = user_factory('db', name='"НавохудоносорВластелинВавилона2"', password='123', plugin = 'Srp')

@pytest.mark.version('>=3')
def test_1(act: Action, tmp_user: User, capsys):

    test_sql = f"""
        set bail on;
        set list on;
        set wng off;
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        revoke all on all from {tmp_user.name};
        grant create table to {tmp_user.name};
        commit;

        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';

        select a.mon$user as who_am_i, c.rdb$character_set_name as my_connection_charset
        from mon$attachments a
        join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
        where a.mon$attachment_id = current_connection;

        create table test(n int);
        commit;

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        select
            p.rdb$user usr_name
            ,p.rdb$grantor grantor
            ,p.rdb$grant_option can_grant
            ,p.rdb$relation_name tab_name
            ,p.rdb$user_type usr_type
            ,p.rdb$object_type obj_type
            ,trim(p.rdb$privilege) priv
        from rdb$user_privileges p
        where
            upper(trim(p.rdb$relation_name)) = upper('test')
            and p.rdb$user = _utf8 '{tmp_user.name}' collate unicode_ci
        order by priv;
        commit;
    """

    expected_stdout = f"""
        WHO_AM_I                        {tmp_user.name.upper()}
        MY_CONNECTION_CHARSET           UTF8

        USR_NAME                        {tmp_user.name.upper()}
        GRANTOR                         {tmp_user.name.upper()}
        CAN_GRANT                       1
        TAB_NAME                        TEST
        USR_TYPE                        8
        OBJ_TYPE                        0
        PRIV                            D

        USR_NAME                        {tmp_user.name.upper()}
        GRANTOR                         {tmp_user.name.upper()}
        CAN_GRANT                       1
        TAB_NAME                        TEST
        USR_TYPE                        8
        OBJ_TYPE                        0
        PRIV                            I

        USR_NAME                        {tmp_user.name.upper()}
        GRANTOR                         {tmp_user.name.upper()}
        CAN_GRANT                       1
        TAB_NAME                        TEST
        USR_TYPE                        8
        OBJ_TYPE                        0
        PRIV                            R

        USR_NAME                        {tmp_user.name.upper()}
        GRANTOR                         {tmp_user.name.upper()}
        CAN_GRANT                       1
        TAB_NAME                        TEST
        USR_TYPE                        8
        OBJ_TYPE                        0
        PRIV                            S

        USR_NAME                        {tmp_user.name.upper()}
        GRANTOR                         {tmp_user.name.upper()}
        CAN_GRANT                       1
        TAB_NAME                        TEST
        USR_TYPE                        8
        OBJ_TYPE                        0
        PRIV                            U
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_sql, charset = 'utf8', connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
